/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ffmpeg_subtitle_stream.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;
using std::map;
using std::list;
using std::cout;
using std::make_pair;
using dcp::raw_convert;

/** Construct a SubtitleStream from a value returned from to_string().
 *  @param node String returned from to_string().
 *  @param version State file version.
 */
FFmpegSubtitleStream::FFmpegSubtitleStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
{
	if (version >= 33) {
		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Colour")) {
			_colours[RGBA(i->node_child("From"))] = RGBA (i->node_child("To"));
		}
	}
}

void
FFmpegSubtitleStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);

	for (map<RGBA, RGBA>::const_iterator i = _colours.begin(); i != _colours.end(); ++i) {
		xmlpp::Node* node = root->add_child("Colour");
		i->first.as_xml (node->add_child("From"));
		i->second.as_xml (node->add_child("To"));
	}
}

map<RGBA, RGBA>
FFmpegSubtitleStream::colours () const
{
	return _colours;
}

void
FFmpegSubtitleStream::set_colour (RGBA from, RGBA to)
{
	_colours[from] = to;
}
