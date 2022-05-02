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


#include <libcxml/cxml.h>


namespace xmlpp {
	class Element;
}


class RoughDuration
{
public:
	enum class Unit {
		DAYS,
		WEEKS,
		MONTHS,
		YEARS
	};

	RoughDuration (int duration_, Unit unit_)
		: duration(duration_)
		, unit(unit_)
	{}

	RoughDuration (cxml::ConstNodePtr node);

	void as_xml (xmlpp::Element* node) const;

	int duration;
	Unit unit;
};


bool
operator== (RoughDuration const& a, RoughDuration const& b);

