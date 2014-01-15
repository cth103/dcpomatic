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

#include <libdcp/subtitle_asset.h>
#include "types.h"
#include "rect.h"
#include "util.h"

class Image;

class Decoded
{
public:
	Decoded ()
		: dcp_time (0)
	{}

	virtual ~Decoded () {}

	virtual void set_dcp_times (VideoFrame, AudioFrame, FrameRateChange, DCPTime) = 0;

	DCPTime dcp_time;
};

/** One frame of video from a VideoDecoder */
class DecodedVideo : public Decoded
{
public:
	DecodedVideo ()
		: eyes (EYES_BOTH)
		, same (false)
		, frame (0)
	{}

	DecodedVideo (boost::shared_ptr<const Image> im, Eyes e, bool s, VideoFrame f)
		: image (im)
		, eyes (e)
		, same (s)
		, frame (f)
	{}

	void set_dcp_times (VideoFrame video_frame_rate, AudioFrame, FrameRateChange frc, DCPTime offset)
	{
		dcp_time = frame * TIME_HZ * frc.factor() / video_frame_rate + offset;
	}
	
	boost::shared_ptr<const Image> image;
	Eyes eyes;
	bool same;
	VideoFrame frame;
};

class DecodedAudio : public Decoded
{
public:
	DecodedAudio (boost::shared_ptr<const AudioBuffers> d, AudioFrame f)
		: data (d)
		, frame (f)
	{}

	void set_dcp_times (VideoFrame, AudioFrame audio_frame_rate, FrameRateChange, DCPTime offset)
	{
		dcp_time = frame * TIME_HZ / audio_frame_rate + offset;
	}
	
	boost::shared_ptr<const AudioBuffers> data;
	AudioFrame frame;
};

class DecodedImageSubtitle : public Decoded
{
public:
	DecodedImageSubtitle ()
		: content_time (0)
		, content_time_to (0)
		, dcp_time_to (0)
	{}

	DecodedImageSubtitle (boost::shared_ptr<Image> im, dcpomatic::Rect<double> r, ContentTime f, ContentTime t)
		: image (im)
		, rect (r)
		, content_time (f)
		, content_time_to (t)
		, dcp_time_to (0)
	{}

	void set_dcp_times (VideoFrame, AudioFrame, FrameRateChange frc, DCPTime offset)
	{
		dcp_time = rint (content_time / frc.speed_up) + offset;
		dcp_time_to = rint (content_time_to / frc.speed_up) + offset;
	}

	boost::shared_ptr<Image> image;
	dcpomatic::Rect<double> rect;
	ContentTime content_time;
	ContentTime content_time_to;
	DCPTime dcp_time_to;
};

class DecodedTextSubtitle : public Decoded
{
public:
	DecodedTextSubtitle ()
		: dcp_time_to (0)
	{}

	DecodedTextSubtitle (std::list<libdcp::Subtitle> s)
		: subs (s)
	{}

	void set_dcp_times (VideoFrame, AudioFrame, FrameRateChange frc, DCPTime offset)
	{
		if (subs.empty ()) {
			return;
		}

		/* Assuming that all subs are at the same time */
		dcp_time = rint (subs.front().in().to_ticks() * 4 * TIME_HZ / frc.speed_up) + offset;
		dcp_time_to = rint (subs.front().out().to_ticks() * 4 * TIME_HZ / frc.speed_up) + offset;
	}

	std::list<libdcp::Subtitle> subs;
	DCPTime dcp_time_to;
};

#endif
