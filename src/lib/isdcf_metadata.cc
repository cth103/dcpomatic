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
	: mastered_luminance (node->optional_string_child ("MasteredLuminance").get_value_or (""))
{

}

void
ISDCFMetadata::as_xml (xmlpp::Node* root) const
{
	root->add_child("MasteredLuminance")->add_child_text (mastered_luminance);
}

bool
operator== (ISDCFMetadata const & a, ISDCFMetadata const & b)
{
        return a.mastered_luminance == b.mastered_luminance;
}
