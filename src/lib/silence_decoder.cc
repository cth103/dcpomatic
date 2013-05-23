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

#include "silence_decoder.h"
#include "null_content.h"
#include "audio_buffers.h"

using std::min;
using boost::shared_ptr;

SilenceDecoder::SilenceDecoder (shared_ptr<const Film> f, shared_ptr<NullContent> c)
	: Decoder (f)
	, AudioDecoder (f, c)
{
	
}

void
SilenceDecoder::pass ()
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	Time const this_time = min (_audio_content->length() - _next_audio, TIME_HZ / 2);
	shared_ptr<AudioBuffers> data (new AudioBuffers (MAX_AUDIO_CHANNELS, film->time_to_audio_frames (this_time)));
	data->make_silent ();
	audio (data, _next_audio);
}

void
SilenceDecoder::seek (Time t)
{
	_next_audio = t;
}

Time
SilenceDecoder::next () const
{
	return _next_audio;
}
