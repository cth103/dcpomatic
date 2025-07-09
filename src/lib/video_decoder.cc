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


/** Called by decoder classes when they have a video frame ready */
void
VideoDecoder::emit(shared_ptr<const Film> film, shared_ptr<const ImageProxy> image, ContentTime time)
{
	if (ignore ()) {
		return;
	}

	auto const afr = _content->active_video_frame_rate(film);
	auto const vft = _content->video->frame_type();

	/* Do some heuristics to try and spot the case where the user sets content to 3D
	 * when it is not.  We try to tell this by looking at the differences in time between
	 * the first few frames.  Real 3D content should have two frames for each timestamp.
	 */
	if (_frame_interval_checker) {
		_frame_interval_checker->feed(time, afr);
		if (_frame_interval_checker->guess() == FrameIntervalChecker::PROBABLY_NOT_3D && vft == VideoFrameType::THREE_D) {
			boost::throw_exception (
				DecodeError(
					fmt::format(
						_("The content file {} is set as 3D but does not appear to contain 3D images.  Please set it to 2D.  "
						  "You can still make a 3D DCP from this content by ticking the 3D option in the DCP video tab."),
						_content->path(0).string()
						)
					)
				);
		}

		if (_frame_interval_checker->guess() != FrameIntervalChecker::AGAIN) {
			_frame_interval_checker.reset ();
		}
	}

	switch (vft) {
	case VideoFrameType::TWO_D:
		Data(ContentVideo(image, time, Eyes::BOTH, Part::WHOLE));
		break;
	case VideoFrameType::THREE_D:
	{
		auto eyes = Eyes::LEFT;
		auto j2k = dynamic_pointer_cast<const J2KImageProxy>(image);
		if (j2k && j2k->eye()) {
			eyes = *j2k->eye() == dcp::Eye::LEFT ? Eyes::LEFT : Eyes::RIGHT;
		}

		Data(ContentVideo(image, time, eyes, Part::WHOLE));
		break;
	}
	case VideoFrameType::THREE_D_ALTERNATE:
	{
		Eyes eyes;
		if (_last_emitted_eyes) {
			eyes = _last_emitted_eyes.get() == Eyes::LEFT ? Eyes::RIGHT : Eyes::LEFT;
		} else {
			/* We don't know what eye this frame is, so just guess */
			auto frame = time.frames_round(_content->video_frame_rate().get_value_or(24));
			eyes = (frame % 2) ? Eyes::RIGHT : Eyes::LEFT;
		}
		Data(ContentVideo(image, time, eyes, Part::WHOLE));
		_last_emitted_eyes = eyes;
		break;
	}
	case VideoFrameType::THREE_D_LEFT_RIGHT:
		Data(ContentVideo(image, time, Eyes::LEFT, Part::LEFT_HALF));
		Data(ContentVideo(image, time, Eyes::RIGHT, Part::RIGHT_HALF));
		break;
	case VideoFrameType::THREE_D_TOP_BOTTOM:
		Data(ContentVideo(image, time, Eyes::LEFT, Part::TOP_HALF));
		Data(ContentVideo(image, time, Eyes::RIGHT, Part::BOTTOM_HALF));
		break;
	case VideoFrameType::THREE_D_LEFT:
		Data(ContentVideo(image, time, Eyes::LEFT, Part::WHOLE));
		break;
	case VideoFrameType::THREE_D_RIGHT:
		Data(ContentVideo(image, time, Eyes::RIGHT, Part::WHOLE));
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	_position = time;
}


void
VideoDecoder::seek ()
{
	_position = boost::none;
	_last_emitted_eyes.reset ();
	_frame_interval_checker.reset (new FrameIntervalChecker());
}
