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

#include "decoder.h"
#include "util.h"

class VideoContent;

class VideoDecoder : public virtual Decoder
{
public:
	VideoDecoder (boost::shared_ptr<const Film>);

	virtual void seek (VideoContent::Frame) = 0;
	virtual void seek_back () = 0;

	/** Emitted when a video frame is ready.
	 *  First parameter is the video image.
	 *  Second parameter is true if the image is the same as the last one that was emitted.
	 *  Third parameter is the frame within our source.
	 */
	boost::signals2::signal<void (boost::shared_ptr<const Image>, bool, VideoContent::Frame)> Video;
	
protected:

	void video (boost::shared_ptr<const Image>, bool, VideoContent::Frame);
	VideoContent::Frame _next_video_frame;
};

#endif
