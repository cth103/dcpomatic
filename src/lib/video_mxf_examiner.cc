/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "video_mxf_content.h"
#include "video_mxf_examiner.h"
#include <dcp/exceptions.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>

using boost::shared_ptr;
using boost::optional;

VideoMXFExaminer::VideoMXFExaminer (shared_ptr<const VideoMXFContent> content)
{
	try {
		_asset.reset (new dcp::MonoPictureAsset (content->path(0)));
	} catch (dcp::MXFFileError& e) {
		/* maybe it's stereo */
	} catch (dcp::DCPReadError& e) {
		/* maybe it's stereo */
	}

	if (!_asset) {
		_asset.reset (new dcp::StereoPictureAsset (content->path(0)));
	}
}

optional<double>
VideoMXFExaminer::video_frame_rate () const
{
	return _asset->frame_rate().as_float ();
}

dcp::Size
VideoMXFExaminer::video_size () const
{
	return _asset->size ();
}

Frame
VideoMXFExaminer::video_length () const
{
	return _asset->intrinsic_duration ();
}

optional<double>
VideoMXFExaminer::sample_aspect_ratio () const
{
	return 1.0;
}

bool
VideoMXFExaminer::yuv () const
{
	return false;
}
