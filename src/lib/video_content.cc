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

#include <iomanip>
#include <libcxml/cxml.h>
#include "video_content.h"
#include "video_examiner.h"
#include "ratio.h"
#include "compose.hpp"

#include "i18n.h"

int const VideoContentProperty::VIDEO_SIZE	 = 0;
int const VideoContentProperty::VIDEO_FRAME_RATE = 1;
int const VideoContentProperty::VIDEO_FRAME_TYPE = 2;
int const VideoContentProperty::VIDEO_CROP	 = 3;
int const VideoContentProperty::VIDEO_RATIO	 = 4;

using std::string;
using std::stringstream;
using std::setprecision;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::optional;

VideoContent::VideoContent (shared_ptr<const Film> f, Time s, VideoContent::Frame len)
	: Content (f, s)
	, _video_length (len)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _ratio (Ratio::from_id ("185"))
{

}

VideoContent::VideoContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _video_length (0)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _ratio (Ratio::from_id ("185"))
{

}

VideoContent::VideoContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: Content (f, node)
{
	_video_length = node->number_child<VideoContent::Frame> ("VideoLength");
	_video_size.width = node->number_child<int> ("VideoWidth");
	_video_size.height = node->number_child<int> ("VideoHeight");
	_video_frame_rate = node->number_child<float> ("VideoFrameRate");
	_video_frame_type = static_cast<VideoFrameType> (node->number_child<int> ("VideoFrameType"));
	_crop.left = node->number_child<int> ("LeftCrop");
	_crop.right = node->number_child<int> ("RightCrop");
	_crop.top = node->number_child<int> ("TopCrop");
	_crop.bottom = node->number_child<int> ("BottomCrop");
	optional<string> r = node->optional_string_child ("Ratio");
	if (r) {
		_ratio = Ratio::from_id (r.get ());
	}
}

void
VideoContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("VideoLength")->add_child_text (lexical_cast<string> (_video_length));
	node->add_child("VideoWidth")->add_child_text (lexical_cast<string> (_video_size.width));
	node->add_child("VideoHeight")->add_child_text (lexical_cast<string> (_video_size.height));
	node->add_child("VideoFrameRate")->add_child_text (lexical_cast<string> (_video_frame_rate));
	node->add_child("VideoFrameType")->add_child_text (lexical_cast<string> (static_cast<int> (_video_frame_type)));
	node->add_child("LeftCrop")->add_child_text (boost::lexical_cast<string> (_crop.left));
	node->add_child("RightCrop")->add_child_text (boost::lexical_cast<string> (_crop.right));
	node->add_child("TopCrop")->add_child_text (boost::lexical_cast<string> (_crop.top));
	node->add_child("BottomCrop")->add_child_text (boost::lexical_cast<string> (_crop.bottom));
	if (_ratio) {
		node->add_child("Ratio")->add_child_text (_ratio->id ());
	}
}

void
VideoContent::take_from_video_examiner (shared_ptr<VideoExaminer> d)
{
	/* These examiner calls could call other content methods which take a lock on the mutex */
	libdcp::Size const vs = d->video_size ();
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

void
VideoContent::set_left_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		
		if (_crop.left == c) {
			return;
		}
		
		_crop.left = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_right_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.right == c) {
			return;
		}
		
		_crop.right = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_top_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.top == c) {
			return;
		}
		
		_crop.top = c;
	}
	
	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_bottom_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_crop.bottom == c) {
			return;
		}
		
		_crop.bottom = c;
	}

	signal_changed (VideoContentProperty::VIDEO_CROP);
}

void
VideoContent::set_ratio (Ratio const * r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_ratio == r) {
			return;
		}

		_ratio = r;
	}

	signal_changed (VideoContentProperty::VIDEO_RATIO);
}

/** @return string which includes everything about how this content looks */
string
VideoContent::identifier () const
{
	stringstream s;
	s << Content::digest()
	  << "_" << crop().left
	  << "_" << crop().right
	  << "_" << crop().top
	  << "_" << crop().bottom;

	if (ratio()) {
		s << "_" << ratio()->id ();
	}

	return s.str ();
}

void
VideoContent::set_video_frame_type (VideoFrameType t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_frame_rate = t;
	}

	signal_changed (VideoContentProperty::VIDEO_FRAME_TYPE);
}
