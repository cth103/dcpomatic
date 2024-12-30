/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>


using std::make_shared;
using std::shared_ptr;
using std::string;


int const SubtitleAnalysis::_current_state_version = 1;


SubtitleAnalysis::SubtitleAnalysis (boost::filesystem::path path)
{
	cxml::Document f ("SubtitleAnalysis");

	f.read_file(dcp::filesystem::fix_long_path(path));

	if (f.optional_number_child<int>("Version").get_value_or(1) < _current_state_version) {
		/* Too old.  Throw an exception so that this analysis is re-run. */
		throw OldFormatError ("Subtitle analysis file is too old");
	}

	cxml::NodePtr bounding_box = f.optional_node_child("BoundingBox");
	if (bounding_box) {
		_bounding_box = dcpomatic::Rect<double> ();
		_bounding_box->x = bounding_box->number_child<double>("X");
		_bounding_box->y = bounding_box->number_child<double>("Y");
		_bounding_box->width = bounding_box->number_child<double>("Width");
		_bounding_box->height = bounding_box->number_child<double>("Height");
	}

	_analysis_x_offset = f.number_child<double>("AnalysisXOffset");
	_analysis_y_offset = f.number_child<double>("AnalysisYOffset");
}


void
SubtitleAnalysis::write (boost::filesystem::path path) const
{
	auto doc = make_shared<xmlpp::Document>();
	xmlpp::Element* root = doc->create_root_node ("SubtitleAnalysis");

	cxml::add_text_child(root, "Version", fmt::to_string(_current_state_version));

	if (_bounding_box) {
		auto bounding_box = cxml::add_child(root, "BoundingBox");
		cxml::add_text_child(bounding_box, "X", fmt::to_string(_bounding_box->x));
		cxml::add_text_child(bounding_box, "Y", fmt::to_string(_bounding_box->y));
		cxml::add_text_child(bounding_box, "Width", fmt::to_string(_bounding_box->width));
		cxml::add_text_child(bounding_box, "Height", fmt::to_string(_bounding_box->height));
	}

	cxml::add_text_child(root, "AnalysisXOffset", fmt::to_string(_analysis_x_offset));
	cxml::add_text_child(root, "AnalysisYOffset", fmt::to_string(_analysis_y_offset));

	doc->write_to_file_formatted (path.string());
}

