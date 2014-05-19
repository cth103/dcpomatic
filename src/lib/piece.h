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

#ifndef DCPOMATIC_PIECE_H
#define DCPOMATIC_PIECE_H

#include "types.h"
#include "video_content.h"

class Content;
class Decoder;
class Piece;
class ImageProxy;
class Player;

struct IncomingVideo
{
public:
	boost::weak_ptr<Piece> weak_piece;
	boost::shared_ptr<const ImageProxy> image;
	Eyes eyes;
	Part part;
	bool same;
	VideoContent::Frame frame;
	Time extra;
};

class Piece
{
public:
	Piece (boost::shared_ptr<Content> c);
	Piece (boost::shared_ptr<Content> c, boost::shared_ptr<Decoder> d);
	void set_repeat (IncomingVideo video, int num);
	void reset_repeat ();
	bool repeating () const;
	void repeat (Player* player);

	boost::shared_ptr<Content> content;
	boost::shared_ptr<Decoder> decoder;
	/** Time of the last video we emitted relative to the start of the DCP */
	Time video_position;
	/** Time of the last audio we emitted relative to the start of the DCP */
	Time audio_position;

	IncomingVideo repeat_video;
	int repeat_to_do;
	int repeat_done;
};

#endif
