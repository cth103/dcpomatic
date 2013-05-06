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

#include <libcxml/cxml.h>
#include "video_content.h"
#include "video_decoder.h"

#include "i18n.h"

int const VideoContentProperty::VIDEO_LENGTH = 0;
int const VideoContentProperty::VIDEO_SIZE = 1;
int const VideoContentProperty::VIDEO_FRAME_RATE = 2;

using std::string;
using std::stringstream;
using std::setprecision;
using boost::shared_ptr;
using boost::lexical_cast;

VideoContent::VideoContent (boost::filesystem::path f)
	: Content (f)
	, _video_length (0)
{

}

VideoContent::VideoContent (shared_ptr<const cxml::Node> node)
	: Content (node)
{
	_video_length = node->number_child<ContentVideoFrame> ("VideoLength");
	_video_size.width = node->number_child<int> ("VideoWidth");
	_video_size.height = node->number_child<int> ("VideoHeight");
	_video_frame_rate = node->number_child<float> ("VideoFrameRate");
}

VideoContent::VideoContent (VideoContent const & o)
	: Content (o)
	, _video_length (o._video_length)
	, _video_size (o._video_size)
	, _video_frame_rate (o._video_frame_rate)
{

}

void
VideoContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("VideoLength")->add_child_text (lexical_cast<string> (_video_length));
	node->add_child("VideoWidth")->add_child_text (lexical_cast<string> (_video_size.width));
	node->add_child("VideoHeight")->add_child_text (lexical_cast<string> (_video_size.height));
	node->add_child("VideoFrameRate")->add_child_text (lexical_cast<string> (_video_frame_rate));
}

void
VideoContent::take_from_video_decoder (shared_ptr<VideoDecoder> d)
{
	/* These decoder calls could call other content methods which take a lock on the mutex */
	libdcp::Size const vs = d->native_size ();
	float const vfr = d->video_frame_rate ();
	
        {
                boost::mutex::scoped_lock lm (_mutex);
                _video_size = vs;
		_video_frame_rate = vfr;
        }
        
        signal_changed (VideoContentProperty::VIDEO_SIZE);
        signal_changed (VideoContentProperty::VIDEO_FRAME_RATE);
}


string
VideoContent::information () const
{
	if (video_size().width == 0 || video_size().height == 0) {
		return "";
	}
	
	stringstream s;

	s << String::compose (
		_("%1x%2 pixels (%3:1)"),
		video_size().width,
		video_size().height,
		setprecision (3), float (video_size().width) / video_size().height
		);
	
	return s.str ();
}

Time
VideoContent::temporal_length () const
{
	return video_length() / video_frame_rate();
}
