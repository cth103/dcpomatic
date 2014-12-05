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

#include "video_content_scale.h"
#include "ratio.h"
#include "safe_stringstream.h"
#include "util.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/optional.hpp>

#include "i18n.h"

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::optional;

vector<VideoContentScale> VideoContentScale::_scales;

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
	SafeStringStream s;
	
	if (_ratio) {
		s << _ratio->id ();
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

VideoContentScale
VideoContentScale::from_id (string id)
{
	Ratio const * r = Ratio::from_id (id);
	if (r) {
		return VideoContentScale (r);
	}

	if (id == "S0") {
		return VideoContentScale (false);
	}

	return VideoContentScale (true);
}
		
/** @param display_container Size of the container that we are displaying this content in.
 *  @param film_container The size of the film's image.
 */
dcp::Size
VideoContentScale::size (shared_ptr<const VideoContent> c, dcp::Size display_container, dcp::Size film_container, int round) const
{
	if (_ratio) {
		return fit_ratio_within (_ratio->ratio (), display_container, round);
	}

	dcp::Size const ac = c->video_size_after_crop ();

	/* Force scale if the film_container is smaller than the content's image */
	if (_scale || film_container.width < ac.width || film_container.height < ac.height) {
		return fit_ratio_within (ac.ratio (), display_container, round);
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
