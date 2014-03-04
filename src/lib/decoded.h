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

#include <dcp/subtitle_string.h>
#include "types.h"
#include "rect.h"
#include "util.h"

class Image;

class Decoded
{
public:
	Decoded ()
		: content_time (0)
		, dcp_time (0)
	{}

	Decoded (ContentTime t)
		: content_time (t)
		, dcp_time (0)
	{}

	virtual ~Decoded () {}

	virtual void set_dcp_times (FrameRateChange frc, DCPTime offset)
	{
		dcp_time = DCPTime (content_time, frc) + offset;
	}

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

	DecodedVideo (ContentTime t, boost::shared_ptr<const Image> im, Eyes e, bool s)
		: Decoded (t)
		, image (im)
		, eyes (e)
		, same (s)
	{}

	boost::shared_ptr<const Image> image;
	Eyes eyes;
	bool same;
};

class DecodedAudio : public Decoded
{
public:
	DecodedAudio (ContentTime t, boost::shared_ptr<const AudioBuffers> d)
		: Decoded (t)
		, data (d)
	{}
	
	boost::shared_ptr<const AudioBuffers> data;
};

class DecodedImageSubtitle : public Decoded
{
public:
	DecodedImageSubtitle ()
		: content_time_to (0)
		, dcp_time_to (0)
	{}

	DecodedImageSubtitle (ContentTime f, ContentTime t, boost::shared_ptr<Image> im, dcpomatic::Rect<double> r)
		: Decoded (f)
		, content_time_to (t)
		, dcp_time_to (0)
		, image (im)
		, rect (r)
	{}

	void set_dcp_times (FrameRateChange frc, DCPTime offset)
	{
		Decoded::set_dcp_times (frc, offset);
		dcp_time_to = DCPTime (content_time_to, frc) + offset;
	}

	ContentTime content_time_to;
	DCPTime dcp_time_to;
	boost::shared_ptr<Image> image;
	dcpomatic::Rect<double> rect;
};

class DecodedTextSubtitle : public Decoded
{
public:
	DecodedTextSubtitle ()
		: content_time_to (0)
		, dcp_time_to (0)
	{}

	/* Assuming that all subs are at the same time */
	DecodedTextSubtitle (std::list<dcp::SubtitleString> s)
		: Decoded (ContentTime::from_seconds (subs.front().in().to_ticks() * 4 / 1000.0))
		, content_time_to (ContentTime::from_seconds (subs.front().out().to_ticks() * 4 / 1000.0))
		, subs (s)
	{
		
	}

	void set_dcp_times (FrameRateChange frc, DCPTime offset)
	{
		Decoded::set_dcp_times (frc, offset);
		dcp_time_to = DCPTime (content_time_to, frc) + offset;
	}

	ContentTime content_time_to;
	DCPTime dcp_time_to;
	std::list<dcp::SubtitleString> subs;
};

#endif
