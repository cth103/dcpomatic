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

VideoDecoder::VideoDecoder (shared_ptr<const Film> f, shared_ptr<const VideoContent> c)
	: Decoder (f)
	, _next_video (0)
	, _video_content (c)
	, _frame_rate_conversion (c->video_frame_rate(), f->dcp_video_frame_rate())
	, _odd (false)
{

}

/** Called by subclasses when some video is ready.
 *  @param image frame to emit.
 *  @param same true if this frame is the same as the last one passed to this call.
 *  @param t Time of the frame within the source.
 */
void
VideoDecoder::video (shared_ptr<Image> image, bool same, Time t)
{
	if (_frame_rate_conversion.skip && _odd) {
		_odd = !_odd;
		return;
	}

	image->crop (_video_content->crop(), true);

	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	libdcp::Size const container_size = film->container()->size (film->full_frame ());
	libdcp::Size const image_size = _video_content->ratio()->size (container_size);
	
	shared_ptr<Image> out = image->scale_and_convert_to_rgb (image_size, film->scaler(), true);

	shared_ptr<Subtitle> sub;
	if (_timed_subtitle && _timed_subtitle->displayed_at (t)) {
		sub = _timed_subtitle->subtitle ();
	}

	if (sub) {
		Rect const tx = subtitle_transformed_area (
			float (image_size.width) / video_size().width,
			float (image_size.height) / video_size().height,
			sub->area(), film->subtitle_offset(), film->subtitle_scale()
			);

		shared_ptr<Image> im = sub->image()->scale (tx.size(), film->scaler(), true);
		out->alpha_blend (im, tx.position());
	}

	if (image_size != container_size) {
		assert (image_size.width <= container_size.width);
		assert (image_size.height <= container_size.height);
		shared_ptr<Image> im (new SimpleImage (PIX_FMT_RGB24, container_size, true));
		im->make_black ();
		im->copy (out, Position ((container_size.width - image_size.width) / 2, (container_size.height - image_size.height) / 2));
		out = im;
	}
		
	Video (out, same, t);

	if (_frame_rate_conversion.repeat) {
		Video (image, true, t + film->video_frames_to_time (1));
		_next_video = t + film->video_frames_to_time (2);
	} else {
		_next_video = t + film->video_frames_to_time (1);
	}

	_odd = !_odd;
}

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
