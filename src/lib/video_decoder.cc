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
#include "options.h"
#include "job.h"

using boost::shared_ptr;
using boost::optional;

VideoDecoder::VideoDecoder (shared_ptr<Film> f, shared_ptr<const DecodeOptions> o, Job* j)
	: Decoder (f, o, j)
	, _video_frame (0)
	, _last_source_frame (0)
{

}

/** Called by subclasses to tell the world that some video data is ready.
 *  We find a subtitle then emit it for listeners.
 *  @param frame to emit.
 */
void
VideoDecoder::emit_video (shared_ptr<Image> image, SourceFrame f)
{
	shared_ptr<Subtitle> sub;
	if (_timed_subtitle && _timed_subtitle->displayed_at (f / _film->frames_per_second())) {
		sub = _timed_subtitle->subtitle ();
	}

	signal_video (image, sub);
	_last_source_frame = f;
}

void
VideoDecoder::repeat_last_video ()
{
	if (!_last_image) {
		_last_image.reset (new SimpleImage (pixel_format(), native_size(), false));
		_last_image->make_black ();
	}

	signal_video (_last_image, _last_subtitle);
}

void
VideoDecoder::signal_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	TIMING ("Decoder emits %1", _video_frame);
	Video (image, sub);
	++_video_frame;

	_last_image = image;
	_last_subtitle = sub;
}

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
VideoDecoder::set_subtitle_stream (shared_ptr<SubtitleStream> s)
{
	_subtitle_stream = s;
}

void
VideoDecoder::set_progress () const
{
	if (_job && _film->length()) {
		_job->set_progress (float (_video_frame) / _film->length().get());
	}
}
