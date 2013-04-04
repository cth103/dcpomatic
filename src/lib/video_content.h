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

#ifndef DVDOMATIC_VIDEO_CONTENT_H
#define DVDOMATIC_VIDEO_CONTENT_H

#include "content.h"
#include "util.h"

class VideoDecoder;

class VideoContentProperty
{
public:
	static int const VIDEO_LENGTH;
	static int const VIDEO_SIZE;
	static int const VIDEO_FRAME_RATE;
};

class VideoContent : public virtual Content
{
public:
	VideoContent (boost::filesystem::path);
	VideoContent (boost::shared_ptr<const cxml::Node>);
	VideoContent (VideoContent const &);

	void as_xml (xmlpp::Node *) const;
	virtual std::string information () const;

	ContentVideoFrame video_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_length;
	}

	libdcp::Size video_size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_size;
	}
	
	float video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_rate;
	}

protected:
	void take_from_video_decoder (boost::shared_ptr<VideoDecoder>);

	ContentVideoFrame _video_length;

private:
	libdcp::Size _video_size;
	float _video_frame_rate;
};

#endif
