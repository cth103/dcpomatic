/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_VIDEO_CONTENT_SCALE_H
#define DCPOMATIC_VIDEO_CONTENT_SCALE_H

#include <dcp/util.h>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace cxml {
	class Node;
}

namespace xmlpp {
	class Node;
}

class Ratio;
class VideoContent;

class VideoContentScale
{
public:
	VideoContentScale ();
	VideoContentScale (Ratio const *);
	VideoContentScale (bool);
	VideoContentScale (boost::shared_ptr<cxml::Node>);

	dcp::Size size (boost::shared_ptr<const VideoContent>, dcp::Size display_container, dcp::Size film_container, int round) const;
	std::string id () const;
	std::string name () const;
	void as_xml (xmlpp::Node *) const;

	Ratio const * ratio () const {
		return _ratio;
	}

	bool scale () const {
		return _scale;
	}

	static void setup_scales ();
	static std::vector<VideoContentScale> all () {
		return _scales;
	}
	static VideoContentScale from_id (std::string id);

private:
	/** a ratio to stretch the content to, or 0 for no stretch */
	Ratio const * _ratio;
	/** true if we want to change the size of the content in any way */
	bool _scale;

	/* If _ratio is 0 and _scale is false there is no scale at all (i.e.
	   the content is used at its original size)
	*/

	static std::vector<VideoContentScale> _scales;
};

bool operator== (VideoContentScale const & a, VideoContentScale const & b);
bool operator!= (VideoContentScale const & a, VideoContentScale const & b);

#endif
