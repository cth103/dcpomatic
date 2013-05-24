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

#include "null_content.h"
#include "film.h"

using boost::shared_ptr;

NullContent::NullContent (shared_ptr<const Film> f, Time s, Time len)
	: Content (f, s)
	, VideoContent (f, s, f->time_to_video_frames (len))
	, AudioContent (f, s)
	, _audio_length (f->time_to_audio_frames (len))
	, _length (len)
{

}

int
NullContent::content_audio_frame_rate () const
{
	return output_audio_frame_rate ();
}
	

int
NullContent::output_audio_frame_rate () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	return film->dcp_audio_frame_rate ();
}

int
NullContent::audio_channels () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	return film->dcp_audio_channels ();
}
	
