/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "video_decoder.h"
#include "image.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<const Film> f, shared_ptr<const VideoContent> c)
	: Decoder (f)
	, _video_content (c)
{

}

/** Called by subclasses when they have a video frame ready */
void
VideoDecoder::video (shared_ptr<const Image> image, bool same, ContentTime time)
{
	switch (_video_content->video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		_pending.push_back (shared_ptr<DecodedVideo> (new DecodedVideo (time, image, EYES_BOTH, same)));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
	{
		int const half = image->size().width / 2;
		_pending.push_back (shared_ptr<DecodedVideo> (new DecodedVideo (time, image->crop (Crop (0, half, 0, 0), true), EYES_LEFT, same)));
		_pending.push_back (shared_ptr<DecodedVideo> (new DecodedVideo (time, image->crop (Crop (half, 0, 0, 0), true), EYES_RIGHT, same)));
		break;
	}
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
	{
		int const half = image->size().height / 2;
		_pending.push_back (shared_ptr<DecodedVideo> (new DecodedVideo (time, image->crop (Crop (0, 0, 0, half), true), EYES_LEFT, same)));
		_pending.push_back (shared_ptr<DecodedVideo> (new DecodedVideo (time, image->crop (Crop (0, 0, half, 0), true), EYES_RIGHT, same)));
		break;
	}
	default:
		assert (false);
	}
}
