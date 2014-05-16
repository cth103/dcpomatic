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

#include <iomanip>
#include <libcxml/cxml.h>
#include <dcp/colour_matrix.h>
#include <dcp/raw_convert.h>
#include "video_content.h"
#include "video_examiner.h"
#include "compose.hpp"
#include "ratio.h"
#include "config.h"
#include "colour_conversion.h"
#include "util.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

int const VideoContentProperty::VIDEO_SIZE	  = 0;
int const VideoContentProperty::VIDEO_FRAME_RATE  = 1;
int const VideoContentProperty::VIDEO_FRAME_TYPE  = 2;
int const VideoContentProperty::VIDEO_CROP	  = 3;
int const VideoContentProperty::VIDEO_SCALE	  = 4;
int const VideoContentProperty::COLOUR_CONVERSION = 5;

using std::string;
using std::stringstream;
using std::setprecision;
using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

vector<VideoContentScale> VideoContentScale::_scales;

VideoContent::VideoContent (shared_ptr<const Film> f)
	: Content (f)
	, _video_length (0)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (Ratio::from_id ("185"))
{
	setup_default_colour_conversion ();
}

VideoContent::VideoContent (shared_ptr<const Film> f, DCPTime s, ContentTime len)
	: Content (f, s)
	, _video_length (len)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (Ratio::from_id ("185"))
{
	setup_default_colour_conversion ();
}

VideoContent::VideoContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _video_length (0)
	, _video_frame_rate (0)
	, _video_frame_type (VIDEO_FRAME_TYPE_2D)
	, _scale (Ratio::from_id ("185"))
{
	setup_default_colour_conversion ();
}

VideoContent::VideoContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node, int version)
	: Content (f, node)
{
	_video_length = ContentTime (node->number_child<int64_t> ("VideoLength"));
	_video_size.width = node->number_child<int> ("VideoWidth");
	_video_size.height = node->number_child<int> ("VideoHeight");
	_video_frame_rate = node->number_child<float> ("VideoFrameRate");
	_video_frame_type = static_cast<VideoFrameType> (node->number_child<int> ("VideoFrameType"));
	_crop.left = node->number_child<int> ("LeftCrop");
	_crop.right = node->number_child<int> ("RightCrop");
	_crop.top = node->number_child<int> ("TopCrop");
	_crop.bottom = node->number_child<int> ("BottomCrop");

	if (version <= 7) {
		optional<string> r = node->optional_string_child ("Ratio");
		if (r) {
			_scale = VideoContentScale (Ratio::from_id (r.get ()));
		}
	} else {
		_scale = VideoContentScale (node->node_child ("Scale"));
	}
	
	_colour_conversion = ColourConversion (node->node_child ("ColourConversion"));
}

VideoContent::VideoContent (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: Content (f, c)
	, _video_length (0)
{
	shared_ptr<VideoContent> ref = dynamic_pointer_cast<VideoContent> (c[0]);
	assert (ref);

	for (size_t i = 0; i < c.size(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (c[i]);

		if (vc->video_size() != ref->video_size()) {
			throw JoinError (_("Content to be joined must have the same picture size."));
		}

		if (vc->video_frame_rate() != ref->video_frame_rate()) {
			throw JoinError (_("Content to be joined must have the same video frame rate."));
		}

		if (vc->video_frame_type() != ref->video_frame_type()) {
			throw JoinError (_("Content to be joined must have the same video frame type."));
		}

		if (vc->crop() != ref->crop()) {
			throw JoinError (_("Content to be joined must have the same crop."));
		}

		if (vc->scale() != ref->scale()) {
			throw JoinError (_("Content to be joined must have the same scale setting."));
		}

		if (vc->colour_conversion() != ref->colour_conversion()) {
			throw JoinError (_("Content to be joined must have the same colour conversion."));
		}

		_video_length += vc->video_length ();
	}

	_video_size = ref->video_size ();
	_video_frame_rate = ref->video_frame_rate ();
	_video_frame_type = ref->video_frame_type ();
	_crop = ref->crop ();
	_scale = ref->scale ();
	_colour_conversion = ref->colour_conversion ();
}

void
VideoContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("VideoLength")->add_child_text (raw_convert<string> (_video_length.get ()));
	node->add_child("VideoWidth")->add_child_text (raw_convert<string> (_video_size.width));
	node->add_child("VideoHeight")->add_child_text (raw_convert<string> (_video_size.height));
	node->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate));
	node->add_child("VideoFrameType")->add_child_text (raw_convert<string> (static_cast<int> (_video_frame_type)));
	node->add_child("LeftCrop")->add_child_text (raw_convert<string> (_crop.left));
	node->add_child("RightCrop")->add_child_text (raw_convert<string> (_crop.right));
	node->add_child("TopCrop")->add_child_text (raw_convert<string> (_crop.top));
	node->add_child("BottomCrop")->add_child_text (raw_convert<string> (_crop.bottom));
	_scale.as_xml (node->add_child("Scale"));
	_colour_conversion.as_xml (node->add_child("ColourConversion"));
}

void
VideoContent::setup_default_colour_conversion ()
{
	_colour_conversion = PresetColourConversion (_("sRGB"), 2.4, true, dcp::colour_matrix::srgb_to_xyz, 2.6).conversion;
}

void
VideoContent::take_from_video_examiner (shared_ptr<VideoExaminer> d)
{
	/* These examiner calls could call other content methods which take a lock on the mutex */
	dcp::Size const vs = d->video_size ();
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
		setprecision (3), video_size().ratio ()
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
VideoContent::set_scale (VideoContentScale s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_scale == s) {
			return;
		}

		_scale = s;
	}

	signal_changed (VideoContentProperty::VIDEO_SCALE);
}

/** @return string which includes everything about how this content looks */
string
VideoContent::identifier () const
{
	stringstream s;
	s << Content::identifier()
	  << "_" << crop().left
	  << "_" << crop().right
	  << "_" << crop().top
	  << "_" << crop().bottom
	  << "_" << scale().id()
	  << "_" << colour_conversion().identifier ();

	return s.str ();
}

void
VideoContent::set_video_frame_type (VideoFrameType t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_frame_type = t;
	}

	signal_changed (VideoContentProperty::VIDEO_FRAME_TYPE);
}

string
VideoContent::technical_summary () const
{
	return String::compose (
		"video: length %1, size %2x%3, rate %4",
		video_length_after_3d_combine().seconds(),
		video_size().width,
		video_size().height,
		video_frame_rate()
		);
}

dcp::Size
VideoContent::video_size_after_3d_split () const
{
	dcp::Size const s = video_size ();
	switch (video_frame_type ()) {
	case VIDEO_FRAME_TYPE_2D:
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
	case VIDEO_FRAME_TYPE_3D_LEFT:
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		return s;
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		return dcp::Size (s.width / 2, s.height);
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		return dcp::Size (s.width, s.height / 2);
	}

	assert (false);
}

void
VideoContent::set_colour_conversion (ColourConversion c)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_colour_conversion = c;
	}

	signal_changed (VideoContentProperty::COLOUR_CONVERSION);
}

/** @return Video size after 3D split and crop */
dcp::Size
VideoContent::video_size_after_crop () const
{
	return crop().apply (video_size_after_3d_split ());
}

/** @param t A time offset from the start of this piece of content.
 *  @return Corresponding time with respect to the content.
 */
ContentTime
VideoContent::dcp_time_to_content_time (DCPTime t) const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	return ContentTime (t, FrameRateChange (video_frame_rate(), film->video_frame_rate()));
}

VideoContentScale::VideoContentScale (Ratio const * r)
	: _ratio (r)
	, _scale (true)
{

}

VideoContentScale::VideoContentScale ()
	: _ratio (0)
	, _scale (false)
{

}

VideoContentScale::VideoContentScale (bool scale)
	: _ratio (0)
	, _scale (scale)
{

}

VideoContentScale::VideoContentScale (shared_ptr<cxml::Node> node)
	: _ratio (0)
	, _scale (true)
{
	optional<string> r = node->optional_string_child ("Ratio");
	if (r) {
		_ratio = Ratio::from_id (r.get ());
	} else {
		_scale = node->bool_child ("Scale");
	}
}

void
VideoContentScale::as_xml (xmlpp::Node* node) const
{
	if (_ratio) {
		node->add_child("Ratio")->add_child_text (_ratio->id ());
	} else {
		node->add_child("Scale")->add_child_text (_scale ? "1" : "0");
	}
}

string
VideoContentScale::id () const
{
	stringstream s;
	
	if (_ratio) {
		s << _ratio->id () << "_";
	} else {
		s << (_scale ? "S1" : "S0");
	}
	
	return s.str ();
}

string
VideoContentScale::name () const
{
	if (_ratio) {
		return _ratio->nickname ();
	}

	if (_scale) {
		return _("No stretch");
	}

	return _("No scale");
}

/** @param display_container Size of the container that we are displaying this content in.
 *  @param film_container The size of the film's image.
 */
dcp::Size
VideoContentScale::size (shared_ptr<const VideoContent> c, dcp::Size display_container, dcp::Size film_container) const
{
	if (_ratio) {
		return fit_ratio_within (_ratio->ratio (), display_container);
	}

	dcp::Size const ac = c->video_size_after_crop ();

	/* Force scale if the film_container is smaller than the content's image */
	if (_scale || film_container.width < ac.width || film_container.height < ac.height) {
		return fit_ratio_within (ac.ratio (), display_container);
	}

	/* Scale the image so that it will be in the right place in film_container, even if display_container is a
	   different size.
	*/
	return dcp::Size (
		c->video_size().width  * float(display_container.width)  / film_container.width,
		c->video_size().height * float(display_container.height) / film_container.height
		);
}

void
VideoContentScale::setup_scales ()
{
	vector<Ratio const *> ratios = Ratio::all ();
	for (vector<Ratio const *>::const_iterator i = ratios.begin(); i != ratios.end(); ++i) {
		_scales.push_back (VideoContentScale (*i));
	}

	_scales.push_back (VideoContentScale (true));
	_scales.push_back (VideoContentScale (false));
}

bool
operator== (VideoContentScale const & a, VideoContentScale const & b)
{
	return (a.ratio() == b.ratio() && a.scale() == b.scale());
}

bool
operator!= (VideoContentScale const & a, VideoContentScale const & b)
{
	return (a.ratio() != b.ratio() || a.scale() != b.scale());
}
