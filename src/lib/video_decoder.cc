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
#ifdef DCPOMATIC_DEBUG
	: test_gaps (0)
	, _video_content (c)
#else
	: _video_content (c)
#endif
{

}

list<ContentVideo>
VideoDecoder::decoded_video (VideoFrame frame)
{
	list<ContentVideo> output;
	
	for (list<ContentVideo>::const_iterator i = _decoded_video.begin(); i != _decoded_video.end(); ++i) {
		if (i->frame == frame) {
			output.push_back (*i);
		}
	}

	return output;
}

/** Get all frames which exist in the content at a given frame index.
 *  @param frame Frame index.
 *  @param accurate true to try hard to return frames at the precise time that was requested, otherwise frames nearby may be returned.
 *  @return Frames; there may be none (if there is no video there), 1 for 2D or 2 for 3D.
 */
list<ContentVideo>
VideoDecoder::get_video (VideoFrame frame, bool accurate)
{
	if (_decoded_video.empty() || (frame < _decoded_video.front().frame || frame > (_decoded_video.back().frame + 1))) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (ContentTime::from_frames (frame, _video_content->video_frame_rate()), accurate);
	}

	list<ContentVideo> dec;

	/* Now enough pass() calls should either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 */
	if (accurate) {
		/* We are being accurate, so we want the right frame.
		 * This could all be one statement but it's split up for clarity.
		 */
		while (true) {
			if (!decoded_video(frame).empty ()) {
				/* We got what we want */
				break;
			}

			if (pass ()) {
				/* The decoder has nothing more for us */
				break;
			}

			if (!_decoded_video.empty() && _decoded_video.front().frame > frame) {
				/* We're never going to get the frame we want.  Perhaps the caller is asking
				 * for a video frame before the content's video starts (if its audio
				 * begins before its video, for example).
				 */
				break;
			}
		}

		dec = decoded_video (frame);
	} else {
		/* Any frame will do: use the first one that comes out of pass() */
		while (_decoded_video.empty() && !pass ()) {}
		if (!_decoded_video.empty ()) {
			dec.push_back (_decoded_video.front ());
		}
	}

	/* Clean up decoded_video */
	while (!_decoded_video.empty() && _decoded_video.front().frame < (frame - 1)) {
		_decoded_video.pop_front ();
	}

	return dec;
}


/** Called by subclasses when they have a video frame ready */
void
VideoDecoder::video (shared_ptr<const Image> image, VideoFrame frame)
{
	/* We should not receive the same thing twice */
	assert (_decoded_video.empty() || frame != _decoded_video.back().frame);

	/* Fill in gaps */
	/* XXX: 3D */

	while (!_decoded_video.empty () && (_decoded_video.back().frame + 1) < frame) {
#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif
		_decoded_video.push_back (
			ContentVideo (
				_decoded_video.back().image,
				_decoded_video.back().eyes,
				_decoded_video.back().frame + 1
				)
			);
	}
	
	switch (_video_content->video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		_decoded_video.push_back (ContentVideo (image, EYES_BOTH, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
		_decoded_video.push_back (ContentVideo (image, (frame % 2) ? EYES_RIGHT : EYES_LEFT, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
	{
		int const half = image->size().width / 2;
		_decoded_video.push_back (ContentVideo (image->crop (Crop (0, half, 0, 0), true), EYES_LEFT, frame));
		_decoded_video.push_back (ContentVideo (image->crop (Crop (half, 0, 0, 0), true), EYES_RIGHT, frame));
		break;
	}
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
	{
		int const half = image->size().height / 2;
		_decoded_video.push_back (ContentVideo (image->crop (Crop (0, 0, 0, half), true), EYES_LEFT, frame));
		_decoded_video.push_back (ContentVideo (image->crop (Crop (0, 0, half, 0), true), EYES_RIGHT, frame));
		break;
	}
	case VIDEO_FRAME_TYPE_3D_LEFT:
		_decoded_video.push_back (ContentVideo (image, EYES_LEFT, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		_decoded_video.push_back (ContentVideo (image, EYES_RIGHT, frame));
		break;
	default:
		assert (false);
	}
}

void
VideoDecoder::seek (ContentTime, bool)
{
	_decoded_video.clear ();
}

