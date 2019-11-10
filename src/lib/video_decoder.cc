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
VideoDecoder::emit (shared_ptr<const Film> film, shared_ptr<const ImageProxy> image, Frame frame)
{
	if (ignore ()) {
		return;
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
		if (!same && _last_emitted_eyes && *_last_emitted_eyes == EYES_LEFT) {
			/* We just got a new frame index but the last frame was left-eye; it looks like
			   this content is not really 3D.
			*/
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
		Eyes const eyes = same ? EYES_RIGHT : EYES_LEFT;
		Data (ContentVideo (image, frame, eyes, PART_WHOLE));
		_last_emitted_frame = frame;
		_last_emitted_eyes = eyes;
		break;
	}
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
		Data (ContentVideo (image, frame / 2, (frame % 2) ? EYES_RIGHT : EYES_LEFT, PART_WHOLE));
		frame /= 2;
		break;
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

	_position = ContentTime::from_frames (frame, _content->active_video_frame_rate(film));
}

void
VideoDecoder::seek ()
{
	_position = boost::optional<ContentTime>();
	_last_emitted_frame.reset ();
	_last_emitted_eyes.reset ();
}
