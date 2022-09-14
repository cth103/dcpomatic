/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "film.h"
#include "frame_interval_checker.h"
#include "image.h"
#include "j2k_image_proxy.h"
#include "log.h"
#include "raw_image_proxy.h"
#include "video_decoder.h"
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using namespace dcpomatic;


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

	auto const afr = _content->active_video_frame_rate(film);
	auto const vft = _content->video->frame_type();

	auto frame_time = ContentTime::from_frames (decoder_frame, afr);

	/* Do some heuristics to try and spot the case where the user sets content to 3D
	 * when it is not.  We try to tell this by looking at the differences in time between
	 * the first few frames.  Real 3D content should have two frames for each timestamp.
	 */
	if (_frame_interval_checker) {
		_frame_interval_checker->feed (frame_time, afr);
		if (_frame_interval_checker->guess() == FrameIntervalChecker::PROBABLY_NOT_3D && vft == VideoFrameType::THREE_D) {
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
	Eyes eyes = Eyes::BOTH;
	if (!_position) {
		/* This is the first data we have received since initialisation or seek.  Set
		   the position based on the frame that was given.  After this first time
		   we just count frames, since (as with audio) it seems that ContentTimes
		   are unreliable from FFmpegDecoder.  They are much better than audio times
		   but still we get the occasional one which is duplicated.  In this case
		   ffmpeg seems to carry on regardless, processing the video frame as normal.
		   If we drop the frame with the duplicated timestamp we obviously lose sync.
		*/

		if (vft == VideoFrameType::THREE_D_ALTERNATE) {
			frame = decoder_frame / 2;
			eyes = (decoder_frame % 2) ? Eyes::RIGHT : Eyes::LEFT;
		} else {
			frame = decoder_frame;
			if (vft == VideoFrameType::THREE_D) {
				auto j2k = dynamic_pointer_cast<const J2KImageProxy>(image);
				/* At the moment only DCP decoders producers VideoFrameType::THREE_D, so only the J2KImageProxy
				 * knows which eye it is.
				 */
				if (j2k && j2k->eye()) {
					eyes = j2k->eye().get() == dcp::Eye::LEFT ? Eyes::LEFT : Eyes::RIGHT;
				}
			}
		}

		_position = ContentTime::from_frames (frame, afr);
	} else {
		if (vft == VideoFrameType::THREE_D) {
			auto j2k = dynamic_pointer_cast<const J2KImageProxy>(image);
			if (j2k && j2k->eye()) {
				if (j2k->eye() == dcp::Eye::LEFT) {
					frame = _position->frames_round(afr) + 1;
					eyes = Eyes::LEFT;
				} else {
					frame = _position->frames_round(afr);
					eyes = Eyes::RIGHT;
				}
			} else {
				/* This should not happen; see above */
				frame = _position->frames_round(afr) + 1;
			}
		} else if (vft == VideoFrameType::THREE_D_ALTERNATE) {
			DCPOMATIC_ASSERT (_last_emitted_eyes);
			if (_last_emitted_eyes.get() == Eyes::RIGHT) {
				frame = _position->frames_round(afr) + 1;
				eyes = Eyes::LEFT;
			} else {
				frame = _position->frames_round(afr);
				eyes = Eyes::RIGHT;
			}
		} else {
			frame = _position->frames_round(afr) + 1;
		}
	}

	switch (vft) {
	case VideoFrameType::TWO_D:
	case VideoFrameType::THREE_D:
		Data (ContentVideo (image, frame, eyes, Part::WHOLE));
		break;
	case VideoFrameType::THREE_D_ALTERNATE:
	{
		Data (ContentVideo (image, frame, eyes, Part::WHOLE));
		_last_emitted_eyes = eyes;
		break;
	}
	case VideoFrameType::THREE_D_LEFT_RIGHT:
		Data (ContentVideo (image, frame, Eyes::LEFT, Part::LEFT_HALF));
		Data (ContentVideo (image, frame, Eyes::RIGHT, Part::RIGHT_HALF));
		break;
	case VideoFrameType::THREE_D_TOP_BOTTOM:
		Data (ContentVideo (image, frame, Eyes::LEFT, Part::TOP_HALF));
		Data (ContentVideo (image, frame, Eyes::RIGHT, Part::BOTTOM_HALF));
		break;
	case VideoFrameType::THREE_D_LEFT:
		Data (ContentVideo (image, frame, Eyes::LEFT, Part::WHOLE));
		break;
	case VideoFrameType::THREE_D_RIGHT:
		Data (ContentVideo (image, frame, Eyes::RIGHT, Part::WHOLE));
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
	_last_emitted_eyes.reset ();
	_frame_interval_checker.reset (new FrameIntervalChecker());
}
