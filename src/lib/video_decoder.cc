/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
#include "subtitle.h"
#include "film.h"
#include "image.h"
#include "ratio.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;

VideoDecoder::VideoDecoder (shared_ptr<const Film> f)
	: Decoder (f)
	, _next_video_frame (0)
{

}

void
VideoDecoder::video (shared_ptr<const Image> image, bool same, VideoContent::Frame frame)
{
        Video (image, same, frame);
	_next_video_frame = frame + 1;
}

#if 0

/** Called by subclasses when a subtitle is ready.
 *  s may be 0 to say that there is no current subtitle.
 *  @param s New current subtitle, or 0.
 */
void
VideoDecoder::subtitle (shared_ptr<TimedSubtitle> s)
{
	_timed_subtitle = s;
	
	if (_timed_subtitle) {
		Position const p = _timed_subtitle->subtitle()->position ();
		_timed_subtitle->subtitle()->set_position (Position (p.x - _video_content->crop().left, p.y - _video_content->crop().top));
	}
}
#endif

