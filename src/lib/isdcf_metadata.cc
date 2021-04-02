/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

#include "isdcf_metadata.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <iostream>

#include "i18n.h"

using std::string;
using std::shared_ptr;
using dcp::raw_convert;

ISDCFMetadata::ISDCFMetadata (cxml::ConstNodePtr node)
	: rating (node->string_child ("Rating"))
	, studio (node->string_child ("Studio"))
	, facility (node->string_child ("Facility"))
	/* This stuff was added later */
	, temp_version (node->optional_bool_child ("TempVersion").get_value_or (false))
	, pre_release (node->optional_bool_child ("PreRelease").get_value_or (false))
	, red_band (node->optional_bool_child ("RedBand").get_value_or (false))
	, chain (node->optional_string_child ("Chain").get_value_or (""))
	, two_d_version_of_three_d (node->optional_bool_child ("TwoDVersionOfThreeD").get_value_or (false))
	, mastered_luminance (node->optional_string_child ("MasteredLuminance").get_value_or (""))
{

}

void
ISDCFMetadata::as_xml (xmlpp::Node* root) const
{
	root->add_child("Rating")->add_child_text (rating);
	root->add_child("Studio")->add_child_text (studio);
	root->add_child("Facility")->add_child_text (facility);
	root->add_child("TempVersion")->add_child_text (temp_version ? "1" : "0");
	root->add_child("PreRelease")->add_child_text (pre_release ? "1" : "0");
	root->add_child("RedBand")->add_child_text (red_band ? "1" : "0");
	root->add_child("Chain")->add_child_text (chain);
	root->add_child("TwoDVersionOfThreeD")->add_child_text (two_d_version_of_three_d ? "1" : "0");
	root->add_child("MasteredLuminance")->add_child_text (mastered_luminance);
}

bool
operator== (ISDCFMetadata const & a, ISDCFMetadata const & b)
{
        return a.rating == b.rating &&
               a.studio == b.studio &&
               a.facility == b.facility &&
               a.temp_version == b.temp_version &&
               a.pre_release == b.pre_release &&
               a.red_band == b.red_band &&
               a.chain == b.chain &&
               a.two_d_version_of_three_d == b.two_d_version_of_three_d &&
               a.mastered_luminance == b.mastered_luminance;
}
