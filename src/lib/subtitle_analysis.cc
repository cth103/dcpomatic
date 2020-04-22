/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_analysis.h"
#include "exceptions.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>

using std::string;
using dcp::raw_convert;
using boost::shared_ptr;

int const SubtitleAnalysis::_current_state_version = 1;


SubtitleAnalysis::SubtitleAnalysis (boost::filesystem::path path)
{
	cxml::Document f ("SubtitleAnalysis");

	f.read_file (path);

	if (f.optional_number_child<int>("Version").get_value_or(1) < _current_state_version) {
		/* Too old.  Throw an exception so that this analysis is re-run. */
		throw OldFormatError ("Audio analysis file is too old");
	}

	cxml::NodePtr bounding_box = f.optional_node_child("BoundingBox");
	if (bounding_box) {
		_bounding_box = dcpomatic::Rect<double> ();
		_bounding_box->x = bounding_box->number_child<double>("X");
		_bounding_box->y = bounding_box->number_child<double>("Y");
		_bounding_box->width = bounding_box->number_child<double>("Width");
		_bounding_box->height = bounding_box->number_child<double>("Height");
	}
}


void
SubtitleAnalysis::write (boost::filesystem::path path) const
{
	shared_ptr<xmlpp::Document> doc (new xmlpp::Document);
	xmlpp::Element* root = doc->create_root_node ("SubtitleAnalysis");

	root->add_child("Version")->add_child_text (raw_convert<string>(_current_state_version));

	if (_bounding_box) {
		xmlpp::Element* bounding_box = root->add_child("BoundingBox");
		bounding_box->add_child("X")->add_child_text(raw_convert<string>(_bounding_box->x));
		bounding_box->add_child("Y")->add_child_text(raw_convert<string>(_bounding_box->y));
		bounding_box->add_child("Width")->add_child_text(raw_convert<string>(_bounding_box->width));
		bounding_box->add_child("Height")->add_child_text(raw_convert<string>(_bounding_box->height));
	}

	doc->write_to_file_formatted (path.string());
}


