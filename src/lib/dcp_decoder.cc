/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/mono_picture_mxf.h>
#include <dcp/stereo_picture_mxf.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_frame.h>

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPDecoder::DCPDecoder (shared_ptr<const DCPContent> c)
	: VideoDecoder (c)
	, AudioDecoder (c)
	, SubtitleDecoder (c)
	, _dcp_content (c)
{
	dcp::DCP dcp (c->directory ());
	dcp.read ();
	if (c->kdm ()) {
		dcp.add (dcp::DecryptedKDM (c->kdm().get (), Config::instance()->decryption_private_key ()));
	}
	DCPOMATIC_ASSERT (dcp.cpls().size() == 1);
	_reels = dcp.cpls().front()->reels ();
	_reel = _reels.begin ();
}

bool
DCPDecoder::pass ()
{
	if (_reel == _reels.end () || !_dcp_content->can_be_played ()) {
		return true;
	}

	float const vfr = _dcp_content->video_frame_rate ();
	int64_t const frame = _next.frames (vfr);
	
	if ((*_reel)->main_picture ()) {
		shared_ptr<dcp::PictureMXF> mxf = (*_reel)->main_picture()->mxf ();
		shared_ptr<dcp::MonoPictureMXF> mono = dynamic_pointer_cast<dcp::MonoPictureMXF> (mxf);
		shared_ptr<dcp::StereoPictureMXF> stereo = dynamic_pointer_cast<dcp::StereoPictureMXF> (mxf);
		int64_t const entry_point = (*_reel)->main_picture()->entry_point ();
		if (mono) {
			video (shared_ptr<ImageProxy> (new J2KImageProxy (mono->get_frame (entry_point + frame), mxf->size())), frame);
		} else {
			video (
				shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame (entry_point + frame), mxf->size(), dcp::EYE_LEFT)),
				frame
				);
			
			video (
				shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame (entry_point + frame), mxf->size(), dcp::EYE_RIGHT)),
				frame
				);
		}
	}

	if ((*_reel)->main_sound ()) {
		int64_t const entry_point = (*_reel)->main_sound()->entry_point ();
		shared_ptr<const dcp::SoundFrame> sf = (*_reel)->main_sound()->mxf()->get_frame (entry_point + frame);
		uint8_t const * from = sf->data ();

		int const channels = _dcp_content->audio_channels ();
		int const frames = sf->size() / (3 * channels);
		shared_ptr<AudioBuffers> data (new AudioBuffers (channels, frames));
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data->data()[j][i] = float (from[0] | (from[1] << 8) | (from[2] << 16)) / (1 << 23);
				from += 3;
			}
		}

		audio (data, _next);
	}

	/* XXX: subtitle */

	_next += ContentTime::from_frames (1, vfr);

	if ((*_reel)->main_picture ()) {
		if (_next.frames (vfr) >= (*_reel)->main_picture()->duration()) {
			++_reel;
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
DCPDecoder::text_subtitles_during (ContentTimePeriod, bool) const
{
	/* XXX */
	return list<ContentTimePeriod> ();
}
