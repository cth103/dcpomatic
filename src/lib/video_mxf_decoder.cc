/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "video_mxf_decoder.h"
#include "video_decoder.h"
#include "video_mxf_content.h"
#include "j2k_image_proxy.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/exceptions.h>

using boost::shared_ptr;

VideoMXFDecoder::VideoMXFDecoder (shared_ptr<const VideoMXFContent> content, shared_ptr<Log> log)
	: _content (content)
{
	video.reset (new VideoDecoder (this, content, log));
}

bool
VideoMXFDecoder::pass (PassReason, bool)
{
	double const vfr = _content->active_video_frame_rate ();
	int64_t const frame = _next.frames_round (vfr);

	if (frame >= _content->video->length()) {
		return true;
	}

	shared_ptr<dcp::MonoPictureAsset> mono;
	try {
		mono.reset (new dcp::MonoPictureAsset (_content->path(0)));
	} catch (dcp::MXFFileError& e) {
		/* maybe it's stereo */
	} catch (dcp::DCPReadError& e) {
		/* maybe it's stereo */
	}

	shared_ptr<dcp::StereoPictureAsset> stereo;
	try {
		stereo.reset (new dcp::StereoPictureAsset (_content->path(0)));
	} catch (dcp::MXFFileError& e) {
		if (!mono) {
			throw;
		}
	} catch (dcp::DCPReadError& e) {
		if (!mono) {
			throw;
		}
	}

	if (mono) {
		video->give (shared_ptr<ImageProxy> (new J2KImageProxy (mono->get_frame(frame), mono->size())), frame);
	} else {
		video->give (shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame(frame), stereo->size(), dcp::EYE_LEFT)), frame);
		video->give (shared_ptr<ImageProxy> (new J2KImageProxy (stereo->get_frame(frame), stereo->size(), dcp::EYE_RIGHT)), frame);
	}

	_next += ContentTime::from_frames (1, vfr);
	return false;
}

void
VideoMXFDecoder::seek (ContentTime t, bool accurate)
{
	video->seek (t, accurate);
	_next = t;
}
