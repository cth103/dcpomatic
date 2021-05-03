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


#include "atmos_content.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS


using std::make_shared;
using std::string;
using std::shared_ptr;


int const AtmosContentProperty::EDIT_RATE = 700;


AtmosContent::AtmosContent (Content* parent)
	: ContentPart (parent)
	, _length (0)
{

}


AtmosContent::AtmosContent (Content* parent, cxml::ConstNodePtr node)
	: ContentPart (parent)
{
	_length = node->number_child<Frame>("AtmosLength");
	_edit_rate = dcp::Fraction (node->string_child("AtmosEditRate"));
}


shared_ptr<AtmosContent>
AtmosContent::from_xml (Content* parent, cxml::ConstNodePtr node)
{
	if (!node->optional_node_child("AtmosLength")) {
		return {};
	}

	return make_shared<AtmosContent>(parent, node);
}


void
AtmosContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("AtmosLength")->add_child_text(dcp::raw_convert<string>(_length));
	node->add_child("AtmosEditRate")->add_child_text(_edit_rate.as_string());
}


void
AtmosContent::set_length (Frame len)
{
	maybe_set (_length, len, ContentProperty::LENGTH);
}


void
AtmosContent::set_edit_rate (dcp::Fraction rate)
{
	maybe_set (_edit_rate, rate, AtmosContentProperty::EDIT_RATE);
}

