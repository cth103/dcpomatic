/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "dcp_decoder.h"
#include "dcp_content.h"
#include "audio_content.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "j2k_image_proxy.h"
#include "subtitle_decoder.h"
#include "image.h"
#include "config.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/stereo_picture_asset_reader.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_frame.h>
#include <dcp/sound_asset_reader.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

DCPDecoder::DCPDecoder (shared_ptr<const DCPContent> c, shared_ptr<Log> log, bool fast)
	: DCP (c)
	, _decode_referenced (false)
{
	video.reset (new VideoDecoder (this, c, log));
	if (c->audio) {
		audio.reset (new AudioDecoder (this, c->audio, log, fast));
	}
	if (c->subtitle) {
		/* XXX: this time here should be the time of the first subtitle, not 0 */
		subtitle.reset (new SubtitleDecoder (this, c->subtitle, log, ContentTime()));
	}

	shared_ptr<dcp::CPL> cpl;
	BOOST_FOREACH (shared_ptr<dcp::CPL> i, cpls ()) {
		if (_dcp_content->cpl() && i->id() == _dcp_content->cpl().get()) {
			cpl = i;
		}
	}

	if (!cpl) {
		/* No CPL found; probably an old file that doesn't specify it;
		   just use the first one.
		*/
		cpl = cpls().front ();
	}

	_reels = cpl->reels ();

	_reel = _reels.begin ();
	_offset = 0;
	get_readers ();
}

bool
DCPDecoder::pass ()
{
	if (_reel == _reels.end () || !_dcp_content->can_be_played ()) {
		return true;
	}

	double const vfr = _dcp_content->active_video_frame_rate ();

	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = _next.frames_round (vfr);

	if ((_mono_reader || _stereo_reader) && (_decode_referenced || !_dcp_content->reference_video())) {
		shared_ptr<dcp::PictureAsset> asset = (*_reel)->main_picture()->asset ();
		int64_t const entry_point = (*_reel)->main_picture()->entry_point ();
		if (_mono_reader) {
			video->emit (
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_mono_reader->get_frame (entry_point + frame),
						asset->size(),
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);
		} else {
			video->emit (
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						asset->size(),
						dcp::EYE_LEFT,
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);

			video->emit (
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						asset->size(),
						dcp::EYE_RIGHT,
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);
		}
	}

	if (_sound_reader && (_decode_referenced || !_dcp_content->reference_audio())) {
		int64_t const entry_point = (*_reel)->main_sound()->entry_point ();
		shared_ptr<const dcp::SoundFrame> sf = _sound_reader->get_frame (entry_point + frame);
		uint8_t const * from = sf->data ();

		int const channels = _dcp_content->audio->stream()->channels ();
		int const frames = sf->size() / (3 * channels);
		shared_ptr<AudioBuffers> data (new AudioBuffers (channels, frames));
		float** data_data = data->data();
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int> ((from[0] << 8) | (from[1] << 16) | (from[2] << 24)) / static_cast<float> (INT_MAX - 256);
				from += 3;
			}
		}

		audio->emit (_dcp_content->audio->stream(), data, ContentTime::from_frames (_offset, vfr) + _next);
	}

	pass_subtitles (_next);

	_next += ContentTime::from_frames (1, vfr);

	if ((*_reel)->main_picture ()) {
		if (_next.frames_round (vfr) >= (*_reel)->main_picture()->duration()) {
			next_reel ();
			_next = ContentTime ();
		}
	}

	return false;
}

void
DCPDecoder::pass_subtitles (ContentTime next)
{
	double const vfr = _dcp_content->active_video_frame_rate ();
	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = next.frames_round (vfr);

	if ((*_reel)->main_subtitle() && (_decode_referenced || !_dcp_content->reference_subtitle())) {
		int64_t const entry_point = (*_reel)->main_subtitle()->entry_point ();
		list<dcp::SubtitleString> subs = (*_reel)->main_subtitle()->asset()->subtitles_during (
			dcp::Time (entry_point + frame, vfr, vfr),
			dcp::Time (entry_point + frame + 1, vfr, vfr),
			true
			);

		if (!subs.empty ()) {
			/* XXX: assuming that all `subs' are at the same time; maybe this is ok */
			subtitle->emit_text (
				ContentTimePeriod (
					ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (subs.front().in().as_seconds ()),
					ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (subs.front().out().as_seconds ())
					),
				subs
				);
		}
	}
}

void
DCPDecoder::next_reel ()
{
	_offset += (*_reel)->main_picture()->duration();
	++_reel;
	get_readers ();
}

void
DCPDecoder::get_readers ()
{
	if (_reel == _reels.end() || !_dcp_content->can_be_played ()) {
		_mono_reader.reset ();
		_stereo_reader.reset ();
		_sound_reader.reset ();
		return;
	}

	if ((*_reel)->main_picture()) {
		shared_ptr<dcp::PictureAsset> asset = (*_reel)->main_picture()->asset ();
		shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (asset);
		shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (asset);
		DCPOMATIC_ASSERT (mono || stereo);
		if (mono) {
			_mono_reader = mono->start_read ();
			_stereo_reader.reset ();
		} else {
			_stereo_reader = stereo->start_read ();
			_mono_reader.reset ();
		}
	} else {
		_mono_reader.reset ();
		_stereo_reader.reset ();
	}

	if ((*_reel)->main_sound()) {
		_sound_reader = (*_reel)->main_sound()->asset()->start_read ();
	} else {
		_sound_reader.reset ();
	}
}

void
DCPDecoder::seek (ContentTime t, bool accurate)
{
	Decoder::seek (t, accurate);

	_reel = _reels.begin ();
	_offset = 0;
	get_readers ();

	if (accurate) {
		int const pre_roll_seconds = 2;

		/* Pre-roll for subs */

		ContentTime pre = t - ContentTime::from_seconds (pre_roll_seconds);
		if (pre < ContentTime()) {
			pre = ContentTime ();
		}

		/* Seek to pre-roll position */

		while (_reel != _reels.end() && pre >= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate ())) {
			pre -= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate ());
			next_reel ();
		}

		/* Pass subtitles in the pre-roll */

		double const vfr = _dcp_content->active_video_frame_rate ();
		for (int i = 0; i < pre_roll_seconds * vfr; ++i) {
			pass_subtitles (pre);
			pre += ContentTime::from_frames (1, vfr);
		}
	}

	/* Seek to correct position */

	while (_reel != _reels.end() && t >= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate ())) {
		t -= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate ());
		next_reel ();
	}

	_next = t;
}

void
DCPDecoder::set_decode_referenced ()
{
	_decode_referenced = true;
}

void
DCPDecoder::set_forced_reduction (optional<int> reduction)
{
	_forced_reduction = reduction;
}
