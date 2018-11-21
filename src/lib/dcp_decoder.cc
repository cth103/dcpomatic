/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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
#include "text_decoder.h"
#include "ffmpeg_image_proxy.h"
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
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_frame.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/subtitle_image.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

DCPDecoder::DCPDecoder (shared_ptr<const DCPContent> c, bool fast)
	: DCP (c)
	, _decode_referenced (false)
{
	if (c->video) {
		video.reset (new VideoDecoder (this, c));
	}
	if (c->audio) {
		audio.reset (new AudioDecoder (this, c->audio, fast));
	}
	BOOST_FOREACH (shared_ptr<TextContent> i, c->text) {
		/* XXX: this time here should be the time of the first subtitle, not 0 */
		text.push_back (shared_ptr<TextDecoder> (new TextDecoder (this, i, ContentTime())));
	}

	list<shared_ptr<dcp::CPL> > cpl_list = cpls ();

	if (cpl_list.empty()) {
		throw DCPError (_("No CPLs found in DCP."));
	}

	shared_ptr<dcp::CPL> cpl;
	BOOST_FOREACH (shared_ptr<dcp::CPL> i, cpl_list) {
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

	set_decode_referenced (false);

	_reels = cpl->reels ();

	_reel = _reels.begin ();
	_offset = 0;
	get_readers ();
}


bool
DCPDecoder::pass (shared_ptr<const Film> film)
{
	if (_reel == _reels.end () || !_dcp_content->can_be_played ()) {
		return true;
	}

	double const vfr = _dcp_content->active_video_frame_rate (film);

	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = _next.frames_round (vfr);

	shared_ptr<dcp::PictureAsset> picture_asset = (*_reel)->main_picture()->asset();
	DCPOMATIC_ASSERT (picture_asset);

	/* We must emit texts first as when we emit the video for this frame
	   it will expect already to have the texts.
	*/
	pass_texts (film, _next, picture_asset->size());

	if ((_mono_reader || _stereo_reader) && (_decode_referenced || !_dcp_content->reference_video())) {
		int64_t const entry_point = (*_reel)->main_picture()->entry_point ();
		if (_mono_reader) {
			video->emit (
				film,
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_mono_reader->get_frame (entry_point + frame),
						picture_asset->size(),
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);
		} else {
			video->emit (
				film,
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						picture_asset->size(),
						dcp::EYE_LEFT,
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);

			video->emit (
				film,
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						picture_asset->size(),
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

		audio->emit (film, _dcp_content->audio->stream(), data, ContentTime::from_frames (_offset, vfr) + _next);
	}

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
DCPDecoder::pass_texts (shared_ptr<const Film> film, ContentTime next, dcp::Size size)
{
	list<shared_ptr<TextDecoder> >::const_iterator decoder = text.begin ();
	if ((*_reel)->main_subtitle()) {
		DCPOMATIC_ASSERT (decoder != text.end ());
		pass_texts (
			film,
			next,
			(*_reel)->main_subtitle()->asset(),
			_dcp_content->reference_text(TEXT_OPEN_SUBTITLE),
			(*_reel)->main_subtitle()->entry_point(),
			*decoder,
			size
			);
		++decoder;
	}
	BOOST_FOREACH (shared_ptr<dcp::ReelClosedCaptionAsset> i, (*_reel)->closed_captions()) {
		DCPOMATIC_ASSERT (decoder != text.end ());
		pass_texts (
			film, next, i->asset(), _dcp_content->reference_text(TEXT_CLOSED_CAPTION), i->entry_point(), *decoder, size
			);
		++decoder;
	}
}

void
DCPDecoder::pass_texts (
	shared_ptr<const Film> film, ContentTime next, shared_ptr<dcp::SubtitleAsset> asset, bool reference, int64_t entry_point, shared_ptr<TextDecoder> decoder, dcp::Size size
	)
{
	double const vfr = _dcp_content->active_video_frame_rate (film);
	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = next.frames_round (vfr);

	if (_decode_referenced || !reference) {
		list<shared_ptr<dcp::Subtitle> > subs = asset->subtitles_during (
			dcp::Time (entry_point + frame, vfr, vfr),
			dcp::Time (entry_point + frame + 1, vfr, vfr),
			true
			);

		list<dcp::SubtitleString> strings;

		BOOST_FOREACH (shared_ptr<dcp::Subtitle> i, subs) {
			shared_ptr<dcp::SubtitleString> is = dynamic_pointer_cast<dcp::SubtitleString> (i);
			if (is) {
				if (!strings.empty() && (strings.back().in() != is->in() || strings.back().out() != is->out())) {
					dcp::SubtitleString b = strings.back();
					decoder->emit_plain (
						ContentTimePeriod (
							ContentTime::from_frames(_offset - entry_point, vfr) + ContentTime::from_seconds(b.in().as_seconds()),
							ContentTime::from_frames(_offset - entry_point, vfr) + ContentTime::from_seconds(b.out().as_seconds())
							),
						strings
						);
					strings.clear ();
				}

				strings.push_back (*is);
			}

			shared_ptr<dcp::SubtitleImage> ii = dynamic_pointer_cast<dcp::SubtitleImage> (i);
			if (ii) {
				FFmpegImageProxy proxy (ii->png_image());
				shared_ptr<Image> image = proxy.image().first;
				/* set up rect with height and width */
				dcpomatic::Rect<double> rect(0, 0, image->size().width / double(size.width), image->size().height / double(size.height));

				/* add in position */

				switch (ii->h_align()) {
				case dcp::HALIGN_LEFT:
					rect.x += ii->h_position();
					break;
				case dcp::HALIGN_CENTER:
					rect.x += 0.5 + ii->h_position() - rect.width / 2;
					break;
				case dcp::HALIGN_RIGHT:
					rect.x += 1 - ii->h_position() - rect.width;
					break;
				}

				switch (ii->v_align()) {
				case dcp::VALIGN_TOP:
					rect.y += ii->v_position();
					break;
				case dcp::VALIGN_CENTER:
					rect.y += 0.5 + ii->v_position() - rect.height / 2;
					break;
				case dcp::VALIGN_BOTTOM:
					rect.y += 1 - ii->v_position() - rect.height;
					break;
				}

				decoder->emit_bitmap (
					ContentTimePeriod (
						ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (i->in().as_seconds ()),
						ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (i->out().as_seconds ())
						),
					image, rect
					);
			}
		}

		if (!strings.empty()) {
			dcp::SubtitleString b = strings.back();
			decoder->emit_plain (
				ContentTimePeriod (
					ContentTime::from_frames(_offset - entry_point, vfr) + ContentTime::from_seconds(b.in().as_seconds()),
					ContentTime::from_frames(_offset - entry_point, vfr) + ContentTime::from_seconds(b.out().as_seconds())
					),
				strings
				);
			strings.clear ();
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
DCPDecoder::seek (shared_ptr<const Film> film, ContentTime t, bool accurate)
{
	if (!_dcp_content->can_be_played ()) {
		return;
	}

	Decoder::seek (film, t, accurate);

	_reel = _reels.begin ();
	_offset = 0;
	get_readers ();

	int const pre_roll_seconds = 2;

	/* Pre-roll for subs */

	ContentTime pre = t - ContentTime::from_seconds (pre_roll_seconds);
	if (pre < ContentTime()) {
		pre = ContentTime ();
	}

	/* Seek to pre-roll position */

	while (_reel != _reels.end() && pre >= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate(film))) {
		ContentTime rd = ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate(film));
		pre -= rd;
		t -= rd;
		next_reel ();
	}

	/* Pass texts in the pre-roll */

	double const vfr = _dcp_content->active_video_frame_rate (film);
	for (int i = 0; i < pre_roll_seconds * vfr; ++i) {
		pass_texts (film, pre, (*_reel)->main_picture()->asset()->size());
		pre += ContentTime::from_frames (1, vfr);
	}

	/* Seek to correct position */

	while (_reel != _reels.end() && t >= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate(film))) {
		t -= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->active_video_frame_rate(film));
		next_reel ();
	}

	_next = t;
}

void
DCPDecoder::set_decode_referenced (bool r)
{
	_decode_referenced = r;

	if (video) {
		video->set_ignore (_dcp_content->reference_video() && !_decode_referenced);
	}
	if (audio) {
		audio->set_ignore (_dcp_content->reference_audio() && !_decode_referenced);
	}
}

void
DCPDecoder::set_forced_reduction (optional<int> reduction)
{
	_forced_reduction = reduction;
}
