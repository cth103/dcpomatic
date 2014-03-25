/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
#include "content_video.h"

#include "i18n.h"

using std::cout;
using std::list;
using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<const VideoContent> c)
	: _video_content (c)
{

}

shared_ptr<ContentVideo>
VideoDecoder::decoded_video (VideoFrame frame)
{
	for (list<shared_ptr<ContentVideo> >::const_iterator i = _decoded_video.begin(); i != _decoded_video.end(); ++i) {
		if ((*i)->frame == frame) {
			return *i;
		}
	}

	return shared_ptr<ContentVideo> ();
}

shared_ptr<ContentVideo>
VideoDecoder::get_video (VideoFrame frame, bool accurate)
{
	if (_decoded_video.empty() || (frame < _decoded_video.front()->frame || frame > (_decoded_video.back()->frame + 1))) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (ContentTime::from_frames (frame, _video_content->video_frame_rate()), accurate);
	}

	shared_ptr<ContentVideo> dec;

	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 */
	if (accurate) {
		/* We are being accurate, so we want the right frame */
		while (!decoded_video (frame) && !pass ()) {}
		dec = decoded_video (frame);
	} else {
		/* Any frame will do: use the first one that comes out of pass() */
		while (_decoded_video.empty() && !pass ()) {}
		if (!_decoded_video.empty ()) {
			dec = _decoded_video.front ();
		}
	}

	/* Clean up decoded_video */
	while (!_decoded_video.empty() && _decoded_video.front()->frame < (frame - 1)) {
		_decoded_video.pop_front ();
	}

	return dec;
}


/** Called by subclasses when they have a video frame ready */
void
VideoDecoder::video (shared_ptr<const Image> image, VideoFrame frame)
{
	switch (_video_content->video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image, EYES_BOTH, frame)));
		break;
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image, (frame % 2) ? EYES_RIGHT : EYES_LEFT, frame)));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
	{
		int const half = image->size().width / 2;
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image->crop (Crop (0, half, 0, 0), true), EYES_LEFT, frame)));
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image->crop (Crop (half, 0, 0, 0), true), EYES_RIGHT, frame)));
		break;
	}
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
	{
		int const half = image->size().height / 2;
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image->crop (Crop (0, 0, 0, half), true), EYES_LEFT, frame)));
		_decoded_video.push_back (shared_ptr<ContentVideo> (new ContentVideo (image->crop (Crop (0, 0, half, 0), true), EYES_RIGHT, frame)));
		break;
	}
	default:
		assert (false);
	}
}

void
VideoDecoder::seek (ContentTime, bool)
{
	_decoded_video.clear ();
}

