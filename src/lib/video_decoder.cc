/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "video_decoder.h"
#include "image.h"
#include "raw_image_proxy.h"
#include "film.h"
#include "log.h"
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::list;
using std::max;
using std::back_inserter;
using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (Decoder* parent, shared_ptr<const Content> c, shared_ptr<Log> log)
#ifdef DCPOMATIC_DEBUG
	: test_gaps (0)
	, _parent (parent),
	  _content (c)
#else
        : _parent (parent)
	, _content (c)
#endif
	, _log (log)
	, _last_seek_accurate (true)
	, _ignore (false)
{
	_black_image.reset (new Image (AV_PIX_FMT_RGB24, _content->video->size(), true));
	_black_image->make_black ();
}

list<ContentVideo>
VideoDecoder::decoded (Frame frame)
{
	list<ContentVideo> output;

	BOOST_FOREACH (ContentVideo const & i, _decoded) {
		if (i.frame.index() == frame) {
			output.push_back (i);
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
VideoDecoder::get (Frame frame, bool accurate)
{
	if (_no_data_frame && frame >= _no_data_frame.get()) {
		return list<ContentVideo> ();
	}

	/* At this stage, if we have get_video()ed before, _decoded will contain the last frame that this
	   method returned (and possibly a few more).  If the requested frame is not in _decoded and it is not the next
	   one after the end of _decoded we need to seek.
	*/

	_log->log (String::compose ("VD has request for %1", frame), LogEntry::TYPE_DEBUG_DECODE);

	if (_decoded.empty() || frame < _decoded.front().frame.index() || frame > (_decoded.back().frame.index() + 1)) {
		_parent->seek (ContentTime::from_frames (frame, _content->active_video_frame_rate()), accurate);
	}

	unsigned int const frames_wanted = _content->video->frame_type() == VIDEO_FRAME_TYPE_2D ? 1 : 2;

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
			if (decoded(frame).size() == frames_wanted) {
				/* We got what we want */
				break;
			}

			if (_parent->pass (Decoder::PASS_REASON_VIDEO, accurate)) {
				/* The decoder has nothing more for us */
				no_data = true;
				break;
			}

			if (!_decoded.empty() && _decoded.front().frame.index() > frame) {
				/* We're never going to get the frame we want.  Perhaps the caller is asking
				 * for a video frame before the content's video starts (if its audio
				 * begins before its video, for example).
				 */
				break;
			}
		}

		dec = decoded (frame);

		if (no_data && dec.empty()) {
			_no_data_frame = frame;
		}

	} else {
		/* Any frame(s) will do: use the first one(s) that comes out of pass() */
		while (_decoded.size() < frames_wanted && !_parent->pass (Decoder::PASS_REASON_VIDEO, accurate)) {}
		list<ContentVideo>::const_iterator i = _decoded.begin();
		unsigned int j = 0;
		while (i != _decoded.end() && j < frames_wanted) {
			dec.push_back (*i);
			++i;
			++j;
		}
	}

	/* Clean up _decoded; keep the frame we are returning, if any (which may have two images
	   for 3D), but nothing before that */
	while (!_decoded.empty() && !dec.empty() && _decoded.front().frame.index() < dec.front().frame.index()) {
		_decoded.pop_front ();
	}

	return dec;
}

/** Fill _decoded from `from' up to, but not including, `to' with
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
	if (!_decoded.empty ()) {
		filler_image = _decoded.back().image;
		filler_part = _decoded.back().part;
	}

	for (Frame i = from; i < to; ++i) {
#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif
		_decoded.push_back (
			ContentVideo (filler_image, VideoFrame (i, eye), filler_part)
			);
	}
}

/** Fill _decoded from `from' up to, but not including, `to'
 *  adding both left and right eye frames.
 */
void
VideoDecoder::fill_both_eyes (VideoFrame from, VideoFrame to)
{
	/* Fill with black... */
	shared_ptr<const ImageProxy> filler_left_image (new RawImageProxy (_black_image));
	shared_ptr<const ImageProxy> filler_right_image (new RawImageProxy (_black_image));
	Part filler_left_part = PART_WHOLE;
	Part filler_right_part = PART_WHOLE;

	/* ...unless there's some video we can fill with */
	for (list<ContentVideo>::const_reverse_iterator i = _decoded.rbegin(); i != _decoded.rend(); ++i) {
		if (i->frame.eyes() == EYES_LEFT && !filler_left_image) {
			filler_left_image = i->image;
			filler_left_part = i->part;
		} else if (i->frame.eyes() == EYES_RIGHT && !filler_right_image) {
			filler_right_image = i->image;
			filler_right_part = i->part;
		}

		if (filler_left_image && filler_right_image) {
			break;
		}
	}

	while (from != to) {

#ifdef DCPOMATIC_DEBUG
		test_gaps++;
#endif

		_decoded.push_back (
			ContentVideo (
				from.eyes() == EYES_LEFT ? filler_left_image : filler_right_image,
				from,
				from.eyes() == EYES_LEFT ? filler_left_part : filler_right_part
				)
			);

		++from;
	}
}

/** Called by decoder classes when they have a video frame ready */
void
VideoDecoder::give (shared_ptr<const ImageProxy> image, Frame frame)
{
	if (_ignore) {
		return;
	}

	_log->log (String::compose ("VD receives %1", frame), LogEntry::TYPE_DEBUG_DECODE);

	/* Work out what we are going to push into _decoded next */
	list<ContentVideo> to_push;
	switch (_content->video->frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_BOTH), PART_WHOLE));
		break;
	case VIDEO_FRAME_TYPE_3D:
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	{
		/* We receive the same frame index twice for 3D-alternate; hence we know which
		   frame this one is.
		*/
		bool const same = (!_decoded.empty() && frame == _decoded.back().frame);
		to_push.push_back (ContentVideo (image, VideoFrame (frame, same ? EYES_RIGHT : EYES_LEFT), PART_WHOLE));
		break;
	}
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_LEFT), PART_LEFT_HALF));
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_RIGHT), PART_RIGHT_HALF));
		break;
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_LEFT), PART_TOP_HALF));
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_RIGHT), PART_BOTTOM_HALF));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT:
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_LEFT), PART_WHOLE));
		break;
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		to_push.push_back (ContentVideo (image, VideoFrame (frame, EYES_RIGHT), PART_WHOLE));
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	/* Now VideoDecoder is required never to have gaps in the frames that it presents
	   via get_video().  Hence we need to fill in any gap between the last thing in _decoded
	   and the things we are about to push.
	*/

	optional<VideoFrame> from;

	if (_decoded.empty() && _last_seek_time && _last_seek_accurate) {
		from = VideoFrame (
			_last_seek_time->frames_round (_content->active_video_frame_rate ()),
			_content->video->frame_type() == VIDEO_FRAME_TYPE_2D ? EYES_BOTH : EYES_LEFT
			);
	} else if (!_decoded.empty ()) {
		from = _decoded.back().frame;
		++(*from);
	}

	/* If we've pre-rolled on a seek we may now receive out-of-order frames
	   (frames before the last seek time) which we can just ignore.
	*/

	if (from && (*from) > to_push.front().frame) {
		return;
	}

	if (from) {
		switch (_content->video->frame_type ()) {
		case VIDEO_FRAME_TYPE_2D:
			fill_one_eye (from->index(), to_push.front().frame.index(), EYES_BOTH);
			break;
		case VIDEO_FRAME_TYPE_3D:
		case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		case VIDEO_FRAME_TYPE_3D_ALTERNATE:
			fill_both_eyes (from.get(), to_push.front().frame);
			break;
		case VIDEO_FRAME_TYPE_3D_LEFT:
			fill_one_eye (from->index(), to_push.front().frame.index(), EYES_LEFT);
			break;
		case VIDEO_FRAME_TYPE_3D_RIGHT:
			fill_one_eye (from->index(), to_push.front().frame.index(), EYES_RIGHT);
			break;
		}
	}

	copy (to_push.begin(), to_push.end(), back_inserter (_decoded));

	/* We can't let this build up too much or we will run out of memory.  There is a
	   `best' value for the allowed size of _decoded which balances memory use
	   with decoding efficiency (lack of seeks).  Throwing away video frames here
	   is not a problem for correctness, so do it.
	*/
	while (_decoded.size() > 96) {
		_decoded.pop_back ();
	}
}

void
VideoDecoder::seek (ContentTime s, bool accurate)
{
	_decoded.clear ();
	_last_seek_time = s;
	_last_seek_accurate = accurate;
}

/** Set this decoder never to produce any data */
void
VideoDecoder::set_ignore ()
{
	_ignore = true;
}
