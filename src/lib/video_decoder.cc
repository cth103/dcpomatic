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
#include "image_proxy.h"
#include "raw_image_proxy.h"
#include "content_video.h"

#include "i18n.h"

using std::cout;
using std::list;
using std::max;
using std::back_inserter;
using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<const VideoContent> c)
#ifdef DCPOMATIC_DEBUG
	: test_gaps (0)
	, _video_content (c)
#else
	: _video_content (c)
#endif
	, _same (false)
{
	_black_image.reset (new Image (PIX_FMT_RGB24, _video_content->video_size(), true));
	_black_image->make_black ();
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
	/* At this stage, if we have get_video()ed before, _decoded_video will contain the last frame that this
	   method returned (and possibly a few more).  If the requested frame is not in _decoded_video and it is not the next
	   one after the end of _decoded_video we need to seek.
	*/
	   
	if (_decoded_video.empty() || frame < _decoded_video.front().frame || frame > (_decoded_video.back().frame + 1)) {
		seek (ContentTime::from_frames (frame, _video_content->video_frame_rate()), accurate);
	}

	list<ContentVideo> dec;

	/* Now enough pass() calls should either:
	 *  (a) give us what we want, or
	 *  (b) give us something after what we want, indicating that we will never get what we want, or
	 *  (c) hit the end of the decoder.
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

	/* Clean up _decoded_video; keep the frame we are returning (which may have two images
	   for 3D), but nothing before that */
	while (!_decoded_video.empty() && _decoded_video.front().frame < dec.front().frame) {
		_decoded_video.pop_front ();
	}

	return dec;
}

/** Fill _decoded_video up to, but not including, the specified frame */
void
VideoDecoder::fill_up_to_2d (VideoFrame frame)
{
	if (frame == 0) {
		/* Already OK */
		return;
	}

	/* Fill with black... */
	boost::shared_ptr<const ImageProxy> filler_image (new RawImageProxy (_black_image));
	Part filler_part = PART_WHOLE;

	/* ...unless there's some video we can fill with */
	if (!_decoded_video.empty ()) {
		filler_image = _decoded_video.back().image;
		filler_part = _decoded_video.back().part;
	}

	VideoFrame filler_frame = _decoded_video.empty() ? 0 : (_decoded_video.back().frame + 1);
	while (filler_frame < frame) {

#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif

		_decoded_video.push_back (
			ContentVideo (filler_image, EYES_BOTH, filler_part, filler_frame)
			);
		
		++filler_frame;
	}
}

/** Fill _decoded_video up to, but not including, the specified frame and eye */
void
VideoDecoder::fill_up_to_3d (VideoFrame frame, Eyes eye)
{
	if (frame == 0 && eye == EYES_LEFT) {
		/* Already OK */
		return;
	}

	/* Fill with black... */
	boost::shared_ptr<const ImageProxy> filler_left_image (new RawImageProxy (_black_image));
	boost::shared_ptr<const ImageProxy> filler_right_image (new RawImageProxy (_black_image));
	Part filler_left_part = PART_WHOLE;
	Part filler_right_part = PART_WHOLE;

	/* ...unless there's some video we can fill with */
	for (list<ContentVideo>::const_reverse_iterator i = _decoded_video.rbegin(); i != _decoded_video.rend(); ++i) {
		if (i->eyes == EYES_LEFT && !filler_left_image) {
			filler_left_image = i->image;
			filler_left_part = i->part;
		} else if (i->eyes == EYES_RIGHT && !filler_right_image) {
			filler_right_image = i->image;
			filler_right_part = i->part;
		}

		if (filler_left_image && filler_right_image) {
			break;
		}
	}

	VideoFrame filler_frame = _decoded_video.empty() ? 0 : _decoded_video.back().frame;
	Eyes filler_eye = _decoded_video.empty() ? EYES_LEFT : _decoded_video.back().eyes;

	if (_decoded_video.empty ()) {
		filler_frame = 0;
		filler_eye = EYES_LEFT;
	} else if (_decoded_video.back().eyes == EYES_LEFT) {
		filler_frame = _decoded_video.back().frame;
		filler_eye = EYES_RIGHT;
	} else if (_decoded_video.back().eyes == EYES_RIGHT) {
		filler_frame = _decoded_video.back().frame + 1;
		filler_eye = EYES_LEFT;
	}

	while (filler_frame != frame || filler_eye != eye) {

#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif

		_decoded_video.push_back (
			ContentVideo (
				filler_eye == EYES_LEFT ? filler_left_image : filler_right_image,
				filler_eye,
				filler_eye == EYES_LEFT ? filler_left_part : filler_right_part,
				filler_frame
				)
			);

		if (filler_eye == EYES_LEFT) {
			filler_eye = EYES_RIGHT;
		} else {
			filler_eye = EYES_LEFT;
			++filler_frame;
		}
	}
}
	
/** Called by subclasses when they have a video frame ready */
void
VideoDecoder::video (shared_ptr<const ImageProxy> image, VideoFrame frame)
{
	/* We may receive the same frame index twice for 3D, and we need to know
	   when that happens.
	*/
	_same = (!_decoded_video.empty() && frame == _decoded_video.back().frame);

	/* Work out what we are going to push into _decoded_video next */
	list<ContentVideo> to_push;
	switch (_video_content->video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		to_push.push_back (ContentVideo (image, EYES_BOTH, PART_WHOLE, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
		to_push.push_back (ContentVideo (image, _same ? EYES_RIGHT : EYES_LEFT, PART_WHOLE, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		to_push.push_back (ContentVideo (image, EYES_LEFT, PART_LEFT_HALF, frame));
		to_push.push_back (ContentVideo (image, EYES_RIGHT, PART_RIGHT_HALF, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		to_push.push_back (ContentVideo (image, EYES_LEFT, PART_TOP_HALF, frame));
		to_push.push_back (ContentVideo (image, EYES_RIGHT, PART_BOTTOM_HALF, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT:
		to_push.push_back (ContentVideo (image, EYES_LEFT, PART_WHOLE, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		to_push.push_back (ContentVideo (image, EYES_RIGHT, PART_WHOLE, frame));
		break;
	default:
		assert (false);
	}

	/* Now VideoDecoder is required never to have gaps in the frames that it presents
	   via get_video().  Hence we need to fill in any gap between the last thing in _decoded_video
	   and the things we are about to push.
	*/

	if (_video_content->video_frame_type() == VIDEO_FRAME_TYPE_2D) {
		fill_up_to_2d (to_push.front().frame);
	} else {
		fill_up_to_3d (to_push.front().frame, to_push.front().eyes);
	}

	copy (to_push.begin(), to_push.end(), back_inserter (_decoded_video));
}

void
VideoDecoder::seek (ContentTime, bool)
{
	_decoded_video.clear ();
}

