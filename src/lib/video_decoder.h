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

#ifndef DCPOMATIC_VIDEO_DECODER_H
#define DCPOMATIC_VIDEO_DECODER_H

#include "video_source.h"
#include "decoder.h"
#include "util.h"

class VideoContent;

class VideoDecoder : public VideoSource, public virtual Decoder
{
public:
	VideoDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const VideoContent>);

	/* Calls for VideoContent to find out about itself */

	/** @return video frame rate second, or 0 if unknown */
	virtual float video_frame_rate () const = 0;
	/** @return video size in pixels */
	virtual libdcp::Size video_size () const = 0;
	/** @return length according to our content's header */
	virtual ContentVideoFrame video_length () const = 0;

protected:
	
	void video (boost::shared_ptr<Image>, bool, Time);
	void subtitle (boost::shared_ptr<TimedSubtitle>);
	bool video_done () const;

	Time _next_video;
	boost::shared_ptr<const VideoContent> _video_content;

private:
	boost::shared_ptr<TimedSubtitle> _timed_subtitle;
	FrameRateConversion _frame_rate_conversion;
	bool _odd;
};

#endif
