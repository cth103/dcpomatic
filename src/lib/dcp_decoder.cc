/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "dcp_decoder.h"
#include "dcp_content.h"
#include "j2k_image_proxy.h"
#include "image.h"
#include "config.h"
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_frame.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPDecoder::DCPDecoder (shared_ptr<const DCPContent> c, bool fast)
	: VideoDecoder (c)
	, AudioDecoder (c, fast)
	, SubtitleDecoder (c)
	, _dcp_content (c)
{
	dcp::DCP dcp (c->directory ());
	dcp.read (false, 0, true);
	if (c->kdm ()) {
		dcp.add (dcp::DecryptedKDM (c->kdm().get (), Config::instance()->decryption_chain()->key().get ()));
	}
	DCPOMATIC_ASSERT (dcp.cpls().size() == 1);
	_reels = dcp.cpls().front()->reels ();
	_reel = _reels.begin ();
}

bool
DCPDecoder::pass (PassReason reason, bool)
{
	if (_reel == _reels.end () || !_dcp_content->can_be_played ()) {
		return true;
	}

	/* Offset of the start of the current reel from the start of the content in frames */
	int offset = 0;
	list<shared_ptr<dcp::Reel> >::const_iterator i = _reels.begin();
	while (i != _reel) {
		offset += (*i)->main_picture()->duration ();
		++i;
	}

	double const vfr = _dcp_content->video_frame_rate ();

	/* Frame within the (played part of the) reel that is coming up next */
	int64_t const frame = _next.frames_round (vfr);

	if ((*_reel)->main_picture () && reason != PASS_REASON_SUBTITLE) {
		shared_ptr<dcp::PictureAsset> asset = (*_reel)->main_picture()->asset ();
		shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (asset);
		shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (asset);
		int64_t const entry_point = (*_reel)->main_picture()->entry_point ();
		if (mono) {
			video (shared_ptr<ImageProxy> (new J2KImageProxy (mono->get_frame (entry_point + frame), asset->size())), offset + frame);
		} else {
			video (
				shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame (entry_point + frame), asset->size(), dcp::EYE_LEFT)),
				offset + frame
				);

			video (
				shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame (entry_point + frame), asset->size(), dcp::EYE_RIGHT)),
				offset + frame
				);
		}
	}

	if ((*_reel)->main_sound () && reason != PASS_REASON_SUBTITLE) {
		int64_t const entry_point = (*_reel)->main_sound()->entry_point ();
		shared_ptr<const dcp::SoundFrame> sf = (*_reel)->main_sound()->asset()->get_frame (entry_point + frame);
		uint8_t const * from = sf->data ();

		int const channels = _dcp_content->audio_stream()->channels ();
		int const frames = sf->size() / (3 * channels);
		shared_ptr<AudioBuffers> data (new AudioBuffers (channels, frames));
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data->data()[j][i] = static_cast<int> ((from[0] << 8) | (from[1] << 16) | (from[2] << 24)) / static_cast<float> (INT_MAX - 256);
				from += 3;
			}
		}

		audio (_dcp_content->audio_stream(), data, ContentTime::from_frames (offset, vfr) + _next);
	}

	if ((*_reel)->main_subtitle ()) {
		int64_t const entry_point = (*_reel)->main_subtitle()->entry_point ();
		list<dcp::SubtitleString> subs = (*_reel)->main_subtitle()->asset()->subtitles_during (
			dcp::Time (entry_point + frame, vfr, vfr),
			dcp::Time (entry_point + frame + 1, vfr, vfr),
			true
			);

		if (!subs.empty ()) {
			/* XXX: assuming that all `subs' are at the same time; maybe this is ok */
			text_subtitle (
				ContentTimePeriod (
					ContentTime::from_frames (offset - entry_point, vfr) + ContentTime::from_seconds (subs.front().in().as_seconds ()),
					ContentTime::from_frames (offset - entry_point, vfr) + ContentTime::from_seconds (subs.front().out().as_seconds ())
					),
				subs
				);
		}
	}

	_next += ContentTime::from_frames (1, vfr);

	if ((*_reel)->main_picture ()) {
		if (_next.frames_round (vfr) >= (*_reel)->main_picture()->duration()) {
			++_reel;
			_next = ContentTime ();
		}
	}

	return false;
}

void
DCPDecoder::seek (ContentTime t, bool accurate)
{
	VideoDecoder::seek (t, accurate);
	AudioDecoder::seek (t, accurate);
	SubtitleDecoder::seek (t, accurate);

	_reel = _reels.begin ();
	while (_reel != _reels.end() && t >= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->video_frame_rate ())) {
		t -= ContentTime::from_frames ((*_reel)->main_picture()->duration(), _dcp_content->video_frame_rate ());
		++_reel;
	}

	_next = t;
}


list<ContentTimePeriod>
DCPDecoder::image_subtitles_during (ContentTimePeriod, bool) const
{
	return list<ContentTimePeriod> ();
}

list<ContentTimePeriod>
DCPDecoder::text_subtitles_during (ContentTimePeriod period, bool starting) const
{
	/* XXX: inefficient */

	list<ContentTimePeriod> ctp;
	double const vfr = _dcp_content->video_frame_rate ();

	BOOST_FOREACH (shared_ptr<dcp::Reel> r, _reels) {
		if (!r->main_subtitle ()) {
			continue;
		}

		int64_t const entry_point = r->main_subtitle()->entry_point ();

		list<dcp::SubtitleString> subs = r->main_subtitle()->asset()->subtitles_during (
			dcp::Time (period.from.seconds(), 1000) - dcp::Time (entry_point, vfr, vfr),
			dcp::Time (period.to.seconds(), 1000) - dcp::Time (entry_point, vfr, vfr),
			starting
			);

		BOOST_FOREACH (dcp::SubtitleString const & s, subs) {
			ctp.push_back (
				ContentTimePeriod (
					ContentTime::from_seconds (s.in().as_seconds ()),
					ContentTime::from_seconds (s.out().as_seconds ())
					)
				);
		}
	}

	return ctp;
}
