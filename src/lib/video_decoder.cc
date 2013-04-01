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
#include "log.h"
#include "job.h"

#include "i18n.h"

using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<const Film> f)
	: Decoder (f)
	, _video_frame (0)
	, _last_source_time (0)
{

}

/** Called by subclasses to tell the world that some video data is ready.
 *  We find a subtitle then emit it for listeners.
 *  @param image frame to emit.
 *  @param t Time of the frame within the source, in seconds.
 */
void
VideoDecoder::emit_video (shared_ptr<Image> image, double t)
{
	shared_ptr<Subtitle> sub;
	if (_timed_subtitle && _timed_subtitle->displayed_at (t)) {
		sub = _timed_subtitle->subtitle ();
	}

	signal_video (image, false, sub);
	_last_source_time = t;
}

/** Called by subclasses to repeat the last video frame that we
 *  passed to emit_video().  If emit_video hasn't yet been called,
 *  we will generate a black frame.
 */
void
VideoDecoder::repeat_last_video ()
{
	if (!_last_image) {
		_last_image.reset (new SimpleImage (pixel_format(), native_size(), true));
		_last_image->make_black ();
	}

	signal_video (_last_image, true, _last_subtitle);
}

/** Emit our signal to say that some video data is ready.
 *  @param image Video frame.
 *  @param same true if `image' is the same as the last one we emitted.
 *  @param sub Subtitle for this frame, or 0.
 */
void
VideoDecoder::signal_video (shared_ptr<Image> image, bool same, shared_ptr<Subtitle> sub)
{
	TIMING (N_("Decoder emits %1"), _video_frame);
	Video (image, same, sub);
	++_video_frame;

	_last_image = image;
	_last_subtitle = sub;
}

/** Set up the current subtitle.  This will be put onto frames that
 *  fit within its time specification.  s may be 0 to say that there
 *  is no current subtitle.
 *  @param s New current subtitle, or 0.
 */
void
VideoDecoder::emit_subtitle (shared_ptr<TimedSubtitle> s)
{
	_timed_subtitle = s;
	
	if (_timed_subtitle) {
		Position const p = _timed_subtitle->subtitle()->position ();
		_timed_subtitle->subtitle()->set_position (Position (p.x - _film->crop().left, p.y - _film->crop().top));
	}
}

void
VideoDecoder::set_progress (Job* j) const
{
	assert (j);

	if (_film->video_length()) {
		j->set_progress (float (_video_frame) / _film->video_length());
	}
}
