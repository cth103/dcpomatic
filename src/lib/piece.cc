/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "piece.h"
#include "player.h"

using boost::shared_ptr;

Piece::Piece (shared_ptr<Content> c)
	: content (c)
	, video_position (c->position ())
	, audio_position (c->position ())
	, repeat_to_do (0)
	, repeat_done (0)
{

}

Piece::Piece (shared_ptr<Content> c, shared_ptr<Decoder> d)
	: content (c)
	, decoder (d)
	, video_position (c->position ())
	, audio_position (c->position ())
	, repeat_to_do (0)
	, repeat_done (0)
{

}

/** Set this piece to repeat a video frame a given number of times */
void
Piece::set_repeat (IncomingVideo video, int num)
{
	repeat_video = video;
	repeat_to_do = num;
	repeat_done = 0;
}

void
Piece::reset_repeat ()
{
	repeat_video.image.reset ();
	repeat_to_do = 0;
	repeat_done = 0;
}

bool
Piece::repeating () const
{
	return repeat_done != repeat_to_do;
}

void
Piece::repeat (Player* player)
{
	player->process_video (
		repeat_video.weak_piece,
		repeat_video.image,
		repeat_video.eyes,
		repeat_video.part,
		repeat_done > 0,
		repeat_video.frame,
		(repeat_done + 1) * (TIME_HZ / player->_film->video_frame_rate ())
		);
	
	++repeat_done;
}

