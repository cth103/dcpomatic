/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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
#include "raw_image_proxy.h"
#include "film.h"
#include "log.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::cout;
using std::list;
using std::max;
using std::back_inserter;
using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<const Content> c, shared_ptr<Log> log)
#ifdef DCPOMATIC_DEBUG
	: test_gaps (0)
	, _video_content (c)
#else
	: _video_content (c)
#endif
	, _log (log)
	, _last_seek_accurate (true)
	, _ignore_video (false)
{
	_black_image.reset (new Image (AV_PIX_FMT_RGB24, _video_content->video->size(), true));
	_black_image->make_black ();
}

list<ContentVideo>
VideoDecoder::decoded_video (Frame frame)
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
VideoDecoder::get_video (Frame frame, bool accurate)
{
	if (_no_data_frame && frame >= _no_data_frame.get()) {
		return list<ContentVideo> ();
	}

	/* At this stage, if we have get_video()ed before, _decoded_video will contain the last frame that this
	   method returned (and possibly a few more).  If the requested frame is not in _decoded_video and it is not the next
	   one after the end of _decoded_video we need to seek.
	*/

	_log->log (String::compose ("VD has request for %1", frame), LogEntry::TYPE_DEBUG_DECODE);

	if (_decoded_video.empty() || frame < _decoded_video.front().frame || frame > (_decoded_video.back().frame + 1)) {
		seek (ContentTime::from_frames (frame, _video_content->active_video_frame_rate()), accurate);
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
		bool no_data = false;

		while (true) {
			if (!decoded_video(frame).empty ()) {
				/* We got what we want */
				break;
			}

			if (pass (PASS_REASON_VIDEO, accurate)) {
				/* The decoder has nothing more for us */
				no_data = true;
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

		if (no_data && dec.empty()) {
			_no_data_frame = frame;
		}

	} else {
		/* Any frame will do: use the first one that comes out of pass() */
		while (_decoded_video.empty() && !pass (PASS_REASON_VIDEO, accurate)) {}
		if (!_decoded_video.empty ()) {
			dec.push_back (_decoded_video.front ());
		}
	}

	/* Clean up _decoded_video; keep the frame we are returning, if any (which may have two images
	   for 3D), but nothing before that */
	while (!_decoded_video.empty() && !dec.empty() && _decoded_video.front().frame < dec.front().frame) {
		_decoded_video.pop_front ();
	}

	return dec;
}

/** Fill _decoded_video from `from' up to, but not including, `to' with
 *  a frame for one particular Eyes value (which could be EYES_BOTH,
 *  EYES_LEFT or EYES_RIGHT)
 */
void
VideoDecoder::fill_one_eye (Frame from, Frame to, Eyes eye)
{
	if (to == 0) {
		/* Already OK */
		return;
	}

	/* Fill with black... */
	shared_ptr<const ImageProxy> filler_image (new RawImageProxy (_black_image));
	Part filler_part = PART_WHOLE;

	/* ...unless there's some video we can fill with */
	if (!_decoded_video.empty ()) {
		filler_image = _decoded_video.back().image;
		filler_part = _decoded_video.back().part;
	}

	for (Frame i = from; i < to; ++i) {
#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif
		_decoded_video.push_back (
			ContentVideo (filler_image, eye, filler_part, i)
			);
	}
}

/** Fill _decoded_video from `from' up to, but not including, `to'
 *  adding both left and right eye frames.
 */
void
VideoDecoder::fill_both_eyes (Frame from, Frame to, Eyes eye)
{
	if (to == 0 && eye == EYES_LEFT) {
		/* Already OK */
		return;
	}

	/* Fill with black... */
	shared_ptr<const ImageProxy> filler_left_image (new RawImageProxy (_black_image));
	shared_ptr<const ImageProxy> filler_right_image (new RawImageProxy (_black_image));
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

	Frame filler_frame = from;
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

	while (filler_frame != to || filler_eye != eye) {

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
VideoDecoder::video (shared_ptr<const ImageProxy> image, Frame frame)
{
	if (_ignore_video) {
		return;
	}

	_log->log (String::compose ("VD receives %1", frame), LogEntry::TYPE_DEBUG_DECODE);

	/* Work out what we are going to push into _decoded_video next */
	list<ContentVideo> to_push;
	switch (_video_content->video->frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		to_push.push_back (ContentVideo (image, EYES_BOTH, PART_WHOLE, frame));
		break;
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	{
		/* We receive the same frame index twice for 3D-alternate; hence we know which
		   frame this one is.
		*/
		bool const same = (!_decoded_video.empty() && frame == _decoded_video.back().frame);
		to_push.push_back (ContentVideo (image, same ? EYES_RIGHT : EYES_LEFT, PART_WHOLE, frame));
		break;
	}
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
		DCPOMATIC_ASSERT (false);
	}

	/* Now VideoDecoder is required never to have gaps in the frames that it presents
	   via get_video().  Hence we need to fill in any gap between the last thing in _decoded_video
	   and the things we are about to push.
	*/

	optional<Frame> from;
	optional<Frame> to;

	if (_decoded_video.empty() && _last_seek_time && _last_seek_accurate) {
		from = _last_seek_time->frames_round (_video_content->active_video_frame_rate ());
		to = to_push.front().frame;
	} else if (!_decoded_video.empty ()) {
		from = _decoded_video.back().frame + 1;
		to = to_push.front().frame;
	}

	/* If we've pre-rolled on a seek we may now receive out-of-order frames
	   (frames before the last seek time) which we can just ignore.
	*/

	if (from && to && from.get() > to.get()) {
		return;
	}

	if (from) {
		switch (_video_content->video->frame_type ()) {
		case VIDEO_FRAME_TYPE_2D:
			fill_one_eye (from.get(), to.get (), EYES_BOTH);
			break;
		case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		case VIDEO_FRAME_TYPE_3D_ALTERNATE:
			fill_both_eyes (from.get(), to.get(), to_push.front().eyes);
			break;
		case VIDEO_FRAME_TYPE_3D_LEFT:
			fill_one_eye (from.get(), to.get (), EYES_LEFT);
			break;
		case VIDEO_FRAME_TYPE_3D_RIGHT:
			fill_one_eye (from.get(), to.get (), EYES_RIGHT);
			break;
		}
	}

	copy (to_push.begin(), to_push.end(), back_inserter (_decoded_video));

	/* We can't let this build up too much or we will run out of memory.  There is a
	   `best' value for the allowed size of _decoded_video which balances memory use
	   with decoding efficiency (lack of seeks).  Throwing away video frames here
	   is not a problem for correctness, so do it.
	*/
	while (_decoded_video.size() > 96) {
		_decoded_video.pop_back ();
	}
}

void
VideoDecoder::seek (ContentTime s, bool accurate)
{
	_decoded_video.clear ();
	_last_seek_time = s;
	_last_seek_accurate = accurate;
}

/** Set this player never to produce any video data */
void
VideoDecoder::set_ignore_video ()
{
	_ignore_video = true;
}
