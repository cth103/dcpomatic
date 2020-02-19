/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "frame_interval_checker.h"
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

VideoDecoder::VideoDecoder (Decoder* parent, shared_ptr<const Content> c)
	: DecoderPart (parent)
	, _content (c)
	, _frame_interval_checker (new FrameIntervalChecker())
{

}

/** Called by decoder classes when they have a video frame ready.
 *  @param frame Frame index within the content; this does not take into account 3D
 *  so for 3D_ALTERNATE this value goes:
 *     0: frame 0 left
 *     1: frame 0 right
 *     2: frame 1 left
 *     3: frame 1 right
 *  and so on.
 */
void
VideoDecoder::emit (shared_ptr<const Film> film, shared_ptr<const ImageProxy> image, Frame decoder_frame)
{
	if (ignore ()) {
		return;
	}

	double const afr = _content->active_video_frame_rate(film);
	VideoFrameType const vft = _content->video->frame_type();

	ContentTime frame_time = ContentTime::from_frames (decoder_frame, afr);

	/* Do some heuristics to try and spot the case where the user sets content to 3D
	 * when it is not.  We try to tell this by looking at the differences in time between
	 * the first few frames.  Real 3D content should have two frames for each timestamp.
	 */
	if (_frame_interval_checker) {
		_frame_interval_checker->feed (frame_time, afr);
		if (_frame_interval_checker->guess() == FrameIntervalChecker::PROBABLY_NOT_3D && _content->video->frame_type() == VIDEO_FRAME_TYPE_3D) {
			boost::throw_exception (
				DecodeError(
					String::compose(
						_("The content file %1 is set as 3D but does not appear to contain 3D images.  Please set it to 2D.  "
						  "You can still make a 3D DCP from this content by ticking the 3D option in the DCP video tab."),
						_content->path(0)
						)
					)
				);
		}

		if (_frame_interval_checker->guess() != FrameIntervalChecker::AGAIN) {
			_frame_interval_checker.reset ();
		}
	}

	Frame frame;
	if (!_position) {
		/* This is the first data we have received since initialisation or seek.  Set
		   the position based on the frame that was given.  After this first time
		   we just cound frames, since (as with audio) it seems that ContentTimes
		   are unreliable from FFmpegDecoder.  They are much better than audio times
		   but still we get the occasional one which is duplicated.  In this case
		   ffmpeg seems to carry on regardless, processing the video frame as normal.
		   If we drop the frame with the duplicated timestamp we obviously lose sync.
		*/
		_position = ContentTime::from_frames (decoder_frame, afr);
		if (vft == VIDEO_FRAME_TYPE_3D) {
			frame = decoder_frame;
			_last_emitted_eyes = EYES_RIGHT;
		} else if (vft == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			frame = decoder_frame / 2;
			_last_emitted_eyes = EYES_RIGHT;
		} else {
			frame = decoder_frame;
		}
	} else {
		if (vft == VIDEO_FRAME_TYPE_3D || vft == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			DCPOMATIC_ASSERT (_last_emitted_eyes);
			if (_last_emitted_eyes.get() == EYES_RIGHT) {
				frame = _position->frames_round(afr) + 1;
			} else {
				frame = _position->frames_round(afr);
			}
		} else {
			frame = _position->frames_round(afr) + 1;
		}
	}

	switch (_content->video->frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
		Data (ContentVideo (image, frame, EYES_BOTH, PART_WHOLE));
		break;
	case VIDEO_FRAME_TYPE_3D:
	{
		/* We should receive the same frame index twice for 3D; hence we know which
		   frame this one is.
		*/
		bool const same = (_last_emitted_frame && _last_emitted_frame.get() == frame);
		Eyes const eyes = same ? EYES_RIGHT : EYES_LEFT;
		Data (ContentVideo (image, frame, eyes, PART_WHOLE));
		_last_emitted_frame = frame;
		_last_emitted_eyes = eyes;
		break;
	}
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	{
		DCPOMATIC_ASSERT (_last_emitted_eyes);
		Eyes const eyes = _last_emitted_eyes.get() == EYES_LEFT ? EYES_RIGHT : EYES_LEFT;
		Data (ContentVideo (image, frame, eyes, PART_WHOLE));
		_last_emitted_eyes = eyes;
		break;
	}
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		Data (ContentVideo (image, frame, EYES_LEFT, PART_LEFT_HALF));
		Data (ContentVideo (image, frame, EYES_RIGHT, PART_RIGHT_HALF));
		break;
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		Data (ContentVideo (image, frame, EYES_LEFT, PART_TOP_HALF));
		Data (ContentVideo (image, frame, EYES_RIGHT, PART_BOTTOM_HALF));
		break;
	case VIDEO_FRAME_TYPE_3D_LEFT:
		Data (ContentVideo (image, frame, EYES_LEFT, PART_WHOLE));
		break;
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		Data (ContentVideo (image, frame, EYES_RIGHT, PART_WHOLE));
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	_position = ContentTime::from_frames (frame, afr);
}

void
VideoDecoder::seek ()
{
	_position = boost::none;
	_last_emitted_frame.reset ();
	_last_emitted_eyes.reset ();
	_frame_interval_checker.reset (new FrameIntervalChecker());
}
