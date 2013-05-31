/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "black_decoder.h"
#include "image.h"
#include "null_content.h"

using boost::shared_ptr;

BlackDecoder::BlackDecoder (shared_ptr<const Film> f, shared_ptr<NullContent> c)
	: Decoder (f)
	, VideoDecoder (f, c)
{

}

void
BlackDecoder::pass ()
{
	if (!_image) {
		_image.reset (new SimpleImage (AV_PIX_FMT_RGB24, video_size(), true));
		_image->make_black ();
		video (_image, false, _next_video);
	} else {
		video (_image, true, _next_video);
	}
}

float
BlackDecoder::video_frame_rate () const
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return 24;
	}

	return f->dcp_video_frame_rate ();
}

ContentVideoFrame
BlackDecoder::video_length () const
{
	return _video_content->length() * video_frame_rate() / TIME_HZ;
}

Time
BlackDecoder::next () const
{
	return _next_video;
}

void
BlackDecoder::seek (Time t)
{
	_next_video = t;
}

void
BlackDecoder::seek_back ()
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return;
	}

	_next_video -= f->video_frames_to_time (2);
}

void
BlackDecoder::seek_forward ()
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return;
	}

	_next_video += f->video_frames_to_time (1);
}

bool
BlackDecoder::done () const
{
	return video_done ();
}

	
