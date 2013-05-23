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

NullContent::NullContent (Time s, Time len, shared_ptr<const Film> f)
	: Content (s)
	, VideoContent (s, f->time_to_video_frames (len))
	, AudioContent (s)
	, _audio_length (f->time_to_audio_frames (len))
	, _content_audio_frame_rate (f->dcp_audio_frame_rate ())
{

}
