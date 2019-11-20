/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
using namespace dcpomatic;

VideoDecoder::VideoDecoder (Decoder* parent, shared_ptr<const Content> c)
	: DecoderPart (parent)
	, _content (c)
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

	/* Before we `re-write' the frame indexes of these incoming data we need to check for
	   the case where the user has some 2D content which they have marked as 3D.  With 3D
	   we should get two frames for each frame index, but in this `bad' case we only get
	   one.  We need to throw an exception if this happens.
	*/

	if (_content->video->frame_type() == VIDEO_FRAME_TYPE_3D) {
		if (_last_threed_frames.size() > 4) {
			_last_threed_frames.erase (_last_threed_frames.begin());
		}
		_last_threed_frames.push_back (decoder_frame);
		if (_last_threed_frames.size() == 4) {
			if (_last_threed_frames[0] != _last_threed_frames[1] || _last_threed_frames[2] != _last_threed_frames[3]) {
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
		}
	}

	double const afr = _content->active_video_frame_rate(film);

	Frame frame;
	Eyes eyes = EYES_BOTH;
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
		if (_content->video->frame_type() == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			frame = decoder_frame / 2;
			_last_emitted_eyes = EYES_RIGHT;
		} else {
			frame = decoder_frame;
		}
	} else {
		VideoFrameType const ft = _content->video->frame_type ();
		if (ft == VIDEO_FRAME_TYPE_3D_ALTERNATE || ft == VIDEO_FRAME_TYPE_3D) {
			DCPOMATIC_ASSERT (_last_emitted_eyes);
			if (_last_emitted_eyes.get() == EYES_RIGHT) {
				frame = _position->frames_round(afr) + 1;
				eyes = EYES_LEFT;
			} else {
				frame = _position->frames_round(afr);
				eyes = EYES_RIGHT;
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
		Data (ContentVideo (image, frame, eyes, PART_WHOLE));
		_last_emitted_frame = frame;
		_last_emitted_eyes = eyes;
		break;
	}
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	{
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
	_position = boost::optional<ContentTime>();
	_last_emitted_frame.reset ();
	_last_emitted_eyes.reset ();
}
