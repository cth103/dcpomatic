/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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

#include "atmos_decoder.h"
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
#include "digester.h"
#include "frame_interval_checker.h"
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
#include <dcp/decrypted_kdm.h>
#include <dcp/reel_atmos_asset.h>
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using std::map;
using std::string;
using std::vector;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using boost::optional;
using namespace dcpomatic;

DCPDecoder::DCPDecoder (shared_ptr<const Film> film, shared_ptr<const DCPContent> c, bool fast, bool tolerant, shared_ptr<DCPDecoder> old)
	: DCP (c, tolerant)
	, Decoder (film)
	, _decode_referenced (false)
{
	if (c->can_be_played()) {
		if (c->video) {
			video.reset (new VideoDecoder (this, c));
		}
		if (c->audio) {
			audio.reset (new AudioDecoder (this, c->audio, fast));
		}
		for (auto i: c->text) {
			/* XXX: this time here should be the time of the first subtitle, not 0 */
			text.push_back (shared_ptr<TextDecoder> (new TextDecoder (this, i, ContentTime())));
		}
		if (c->atmos) {
			atmos.reset (new AtmosDecoder (this, c));
		}
	}

	/* We try to avoid re-scanning the DCP's files every time we make a new DCPDecoder; we do this
	   by re-using the _reels list.  Before we do this we need to check that nothing too serious
	   has changed in the DCPContent.

	   We do this by storing a digest of the important bits of the DCPContent and then checking that's
	   the same before we re-use _reels.
	*/

	_lazy_digest = calculate_lazy_digest (c);

	if (old && old->lazy_digest() == _lazy_digest) {
		_reels = old->_reels;
	} else {

		list<shared_ptr<dcp::CPL> > cpl_list = cpls ();

		if (cpl_list.empty()) {
			throw DCPError (_("No CPLs found in DCP."));
		}

		shared_ptr<dcp::CPL> cpl;
		for (auto i: cpl_list) {
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
	}

	set_decode_referenced (false);

	_reel = _reels.begin ();
	_offset = 0;
	get_readers ();
}


bool
DCPDecoder::pass ()
{
	if (!_dcp_content->can_be_played()) {
		return true;
	}

	if (_reel == _reels.end()) {
		if (audio) {
			audio->flush ();
		}
		return true;
	}

	double const vfr = _dcp_content->active_video_frame_rate (film());

	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = _next.frames_round (vfr);

	shared_ptr<dcp::PictureAsset> picture_asset = (*_reel)->main_picture()->asset();
	DCPOMATIC_ASSERT (picture_asset);

	/* We must emit texts first as when we emit the video for this frame
	   it will expect already to have the texts.
	*/
	pass_texts (_next, picture_asset->size());

	if ((_mono_reader || _stereo_reader) && (_decode_referenced || !_dcp_content->reference_video())) {
		int64_t const entry_point = (*_reel)->main_picture()->entry_point().get_value_or(0);
		if (_mono_reader) {
			video->emit (
				film(),
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
				film(),
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						picture_asset->size(),
						dcp::Eye::LEFT,
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);

			video->emit (
				film(),
				shared_ptr<ImageProxy> (
					new J2KImageProxy (
						_stereo_reader->get_frame (entry_point + frame),
						picture_asset->size(),
						dcp::Eye::RIGHT,
						AV_PIX_FMT_XYZ12LE,
						_forced_reduction
						)
					),
				_offset + frame
				);
		}
	}

	if (_sound_reader && (_decode_referenced || !_dcp_content->reference_audio())) {
		int64_t const entry_point = (*_reel)->main_sound()->entry_point().get_value_or(0);
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

		audio->emit (film(), _dcp_content->audio->stream(), data, ContentTime::from_frames (_offset, vfr) + _next);
	}

	if (_atmos_reader) {
		DCPOMATIC_ASSERT (_atmos_metadata);
		int64_t const entry_point = (*_reel)->atmos()->entry_point().get_value_or(0);
		atmos->emit (film(), _atmos_reader->get_frame(entry_point + frame), _offset + frame, *_atmos_metadata);
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
DCPDecoder::pass_texts (ContentTime next, dcp::Size size)
{
	list<shared_ptr<TextDecoder> >::const_iterator decoder = text.begin ();
	if (decoder == text.end()) {
		/* It's possible that there is now a main subtitle but no TextDecoders, for example if
		   the CPL has just changed but the TextContent's texts have not been recreated yet.
		*/
		return;
	}

	if ((*_reel)->main_subtitle()) {
		pass_texts (
			next,
			(*_reel)->main_subtitle()->asset(),
			_dcp_content->reference_text(TEXT_OPEN_SUBTITLE),
			(*_reel)->main_subtitle()->entry_point().get_value_or(0),
			*decoder,
			size
			);
		++decoder;
	}

	for (auto i: (*_reel)->closed_captions()) {
		pass_texts (
			next, i->asset(), _dcp_content->reference_text(TEXT_CLOSED_CAPTION), i->entry_point().get_value_or(0), *decoder, size
			);
		++decoder;
	}
}

void
DCPDecoder::pass_texts (
	ContentTime next, shared_ptr<dcp::SubtitleAsset> asset, bool reference, int64_t entry_point, shared_ptr<TextDecoder> decoder, dcp::Size size
	)
{
	double const vfr = _dcp_content->active_video_frame_rate (film());
	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = next.frames_round (vfr);

	if (_decode_referenced || !reference) {
		auto subs = asset->subtitles_during (
			dcp::Time (entry_point + frame, vfr, vfr),
			dcp::Time (entry_point + frame + 1, vfr, vfr),
			true
			);

		list<dcp::SubtitleString> strings;

		for (auto i: subs) {
			auto is = dynamic_pointer_cast<const dcp::SubtitleString>(i);
			if (is) {
				if (!strings.empty() && (strings.back().in() != is->in() || strings.back().out() != is->out())) {
					auto b = strings.back();
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

			/* XXX: perhaps these image subs should also be collected together like the string ones are;
			   this would need to be done both here and in DCPSubtitleDecoder.
			*/

			auto ii = dynamic_pointer_cast<const dcp::SubtitleImage>(i);
			if (ii) {
				emit_subtitle_image (
					ContentTimePeriod (
						ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (i->in().as_seconds ()),
						ContentTime::from_frames (_offset - entry_point, vfr) + ContentTime::from_seconds (i->out().as_seconds ())
						),
					*ii,
					size,
					decoder
					);
			}
		}

		if (!strings.empty()) {
			auto b = strings.back();
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
	_offset += (*_reel)->main_picture()->actual_duration();
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
		_atmos_reader.reset ();
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

	if ((*_reel)->atmos()) {
		shared_ptr<dcp::AtmosAsset> asset = (*_reel)->atmos()->asset();
		_atmos_reader = asset->start_read();
		_atmos_metadata = AtmosMetadata (asset);
	} else {
		_atmos_reader.reset ();
		_atmos_metadata = boost::none;
	}
}

void
DCPDecoder::seek (ContentTime t, bool accurate)
{
	if (!_dcp_content->can_be_played ()) {
		return;
	}

	Decoder::seek (t, accurate);

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

	while (
		_reel != _reels.end() &&
		pre >= ContentTime::from_frames ((*_reel)->main_picture()->actual_duration(), _dcp_content->active_video_frame_rate(film()))
		) {

		ContentTime rd = ContentTime::from_frames ((*_reel)->main_picture()->actual_duration(), _dcp_content->active_video_frame_rate(film()));
		pre -= rd;
		t -= rd;
		next_reel ();
	}

	/* Pass texts in the pre-roll */

	double const vfr = _dcp_content->active_video_frame_rate (film());
	for (int i = 0; i < pre_roll_seconds * vfr; ++i) {
		pass_texts (pre, (*_reel)->main_picture()->asset()->size());
		pre += ContentTime::from_frames (1, vfr);
	}

	/* Seek to correct position */

	while (
		_reel != _reels.end() &&
		t >= ContentTime::from_frames ((*_reel)->main_picture()->actual_duration(), _dcp_content->active_video_frame_rate(film()))
		) {

		t -= ContentTime::from_frames ((*_reel)->main_picture()->actual_duration(), _dcp_content->active_video_frame_rate(film()));
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

string
DCPDecoder::calculate_lazy_digest (shared_ptr<const DCPContent> c) const
{
	Digester d;
	for (auto i: c->paths()) {
		d.add (i.string());
	}
	if (_dcp_content->kdm()) {
		d.add(_dcp_content->kdm()->id());
	}
	d.add (static_cast<bool>(c->cpl()));
	if (c->cpl()) {
		d.add (c->cpl().get());
	}
	return d.get ();
}

ContentTime
DCPDecoder::position () const
{
	return ContentTime::from_frames(_offset, _dcp_content->active_video_frame_rate(film())) + _next;
}


vector<FontData>
DCPDecoder::fonts () const
{
	vector<FontData> data;
	for (auto i: _reels) {
		if (i->main_subtitle() && i->main_subtitle()->asset()) {
			map<string, dcp::ArrayData> fm = i->main_subtitle()->asset()->font_data();
			for (map<string, dcp::ArrayData>::const_iterator j = fm.begin(); j != fm.end(); ++j) {
				data.push_back (FontData(j->first, j->second));
			}
		}
	}
	return data;
}

