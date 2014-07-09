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

#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/mono_picture_mxf.h>
#include <dcp/stereo_picture_mxf.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include "dcp_decoder.h"
#include "dcp_content.h"
#include "image_proxy.h"
#include "image.h"

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPDecoder::DCPDecoder (shared_ptr<const DCPContent> c, shared_ptr<Log> log)
	: VideoDecoder (c)
	, AudioDecoder (c)
	, SubtitleDecoder (c)
	, _log (log)
	, _dcp_content (c)
{
	dcp::DCP dcp (c->directory ());
	dcp.read ();
	assert (dcp.cpls().size() == 1);
	_reels = dcp.cpls().front()->reels ();
	_reel = _reels.begin ();
}

bool
DCPDecoder::pass ()
{
	if (_reel == _reels.end ()) {
		return true;
	}

	float const vfr = _dcp_content->video_frame_rate ();
	
	if ((*_reel)->main_picture ()) {
		shared_ptr<dcp::PictureMXF> mxf = (*_reel)->main_picture()->mxf ();
		shared_ptr<dcp::MonoPictureMXF> mono = dynamic_pointer_cast<dcp::MonoPictureMXF> (mxf);
		shared_ptr<dcp::StereoPictureMXF> stereo = dynamic_pointer_cast<dcp::StereoPictureMXF> (mxf);
		if (mono) {
			shared_ptr<Image> image (new Image (PIX_FMT_RGB24, mxf->size(), false));
			mono->get_frame (_next.frames (vfr))->rgb_frame (image->data()[0]);
			shared_ptr<Image> aligned (new Image (image, true));
			video (shared_ptr<ImageProxy> (new RawImageProxy (aligned, _log)), _next.frames (vfr));
		} else {

			shared_ptr<Image> left (new Image (PIX_FMT_RGB24, mxf->size(), false));
			stereo->get_frame (_next.frames (vfr))->rgb_frame (dcp::EYE_LEFT, left->data()[0]);
			shared_ptr<Image> aligned_left (new Image (left, true));
			video (shared_ptr<ImageProxy> (new RawImageProxy (aligned_left, _log)), _next.frames (vfr));

			shared_ptr<Image> right (new Image (PIX_FMT_RGB24, mxf->size(), false));
			stereo->get_frame (_next.frames (vfr))->rgb_frame (dcp::EYE_RIGHT, right->data()[0]);
			shared_ptr<Image> aligned_right (new Image (right, true));
			video (shared_ptr<ImageProxy> (new RawImageProxy (aligned_right, _log)), _next.frames (vfr));
		}
	}

	/* XXX: sound */
	/* XXX: subtitle */

	_next += ContentTime::from_frames (1, vfr);

	if ((*_reel)->main_picture ()) {
		if ((*_reel)->main_picture()->duration() >= _next.frames (vfr)) {
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
DCPDecoder::subtitles_during (ContentTimePeriod, bool starting) const
{
	return list<ContentTimePeriod> ();
}
