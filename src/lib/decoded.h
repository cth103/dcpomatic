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

#ifndef DCPOMATIC_LIB_DECODED_H
#define DCPOMATIC_LIB_DECODED_H

#include "types.h"
#include "rect.h"

class Image;

class Decoded
{
public:
	Decoded ()
		: content_time (0)
		, dcp_time (0)
	{}

	Decoded (DCPTime ct)
		: content_time (ct)
		, dcp_time (0)
	{}

	virtual ~Decoded () {}

	virtual void set_dcp_times (float speed_up, DCPTime offset) = 0;

	ContentTime content_time;
	DCPTime dcp_time;
};

/** One frame of video from a VideoDecoder */
class DecodedVideo : public Decoded
{
public:
	DecodedVideo ()
		: eyes (EYES_BOTH)
		, same (false)
	{}

	DecodedVideo (boost::shared_ptr<const Image> im, Eyes e, bool s, ContentTime ct)
		: Decoded (ct)
		, image (im)
		, eyes (e)
		, same (s)
	{}

	void set_dcp_times (float speed_up, DCPTime offset) {
		dcp_time = rint (content_time / speed_up) + offset;
	}
	
	boost::shared_ptr<const Image> image;
	Eyes eyes;
	bool same;
};

class DecodedAudio : public Decoded
{
public:
	DecodedAudio (boost::shared_ptr<const AudioBuffers> d, ContentTime ct)
		: Decoded (ct)
		, data (d)
	{}

	void set_dcp_times (float speed_up, DCPTime offset) {
		dcp_time = rint (content_time / speed_up) + offset;
	}
	
	boost::shared_ptr<const AudioBuffers> data;
};

class DecodedSubtitle : public Decoded
{
public:
	DecodedSubtitle ()
		: content_time_to (0)
		, dcp_time_to (0)
	{}

	/* XXX: content/dcp time here */
	DecodedSubtitle (boost::shared_ptr<Image> im, dcpomatic::Rect<double> r, DCPTime f, DCPTime t)
		: Decoded (f)
		, image (im)
		, rect (r)
		, content_time_to (t)
		, dcp_time_to (t)
	{}

	void set_dcp_times (float speed_up, DCPTime offset) {
		dcp_time = rint (content_time / speed_up) + offset;
		dcp_time_to = rint (content_time_to / speed_up) + offset;
	}

	boost::shared_ptr<Image> image;
	dcpomatic::Rect<double> rect;
	ContentTime content_time_to;
	DCPTime dcp_time_to;
};

#endif
