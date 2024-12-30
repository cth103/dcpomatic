/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "rough_duration.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>


using std::string;


RoughDuration::RoughDuration (cxml::ConstNodePtr node)
	: duration(dcp::raw_convert<int>(node->content()))
{
	auto const unit_name = node->string_attribute("unit");
	if (unit_name == "days") {
		unit = Unit::DAYS;
	} else if (unit_name == "weeks") {
		unit = Unit::WEEKS;
	} else if (unit_name == "months") {
		unit = Unit::MONTHS;
	} else if (unit_name == "years") {
		unit = Unit::YEARS;
	} else {
		DCPOMATIC_ASSERT (false);
	}
}


void
RoughDuration::as_xml (xmlpp::Element* node) const
{
	node->add_child_text(fmt::to_string(duration));

	switch (unit) {
	case Unit::DAYS:
		node->set_attribute("unit", "days");
		break;
	case Unit::WEEKS:
		node->set_attribute("unit", "weeks");
		break;
	case Unit::MONTHS:
		node->set_attribute("unit", "months");
		break;
	case Unit::YEARS:
		node->set_attribute("unit", "years");
		break;
	}
}


bool
operator== (RoughDuration const& a, RoughDuration const& b)
{
	return a.duration == b.duration && a.unit == b.unit;
}

