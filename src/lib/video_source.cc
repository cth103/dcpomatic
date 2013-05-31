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

#include "video_source.h"
#include "video_sink.h"

using boost::shared_ptr;
using boost::weak_ptr;
using boost::bind;

static void
process_video_proxy (weak_ptr<VideoSink> sink, shared_ptr<const Image> image, bool same, Time time)
{
	shared_ptr<VideoSink> p = sink.lock ();
	if (p) {
		p->process_video (image, same, time);
	}
}

void
VideoSource::connect_video (shared_ptr<VideoSink> s)
{
	/* If we bind, say, a Player (as the VideoSink) to a Decoder (which is owned
	   by the Player) we create a cycle.  Use a weak_ptr to break it.
	*/
	Video.connect (bind (process_video_proxy, weak_ptr<VideoSink> (s), _1, _2, _3));
}

