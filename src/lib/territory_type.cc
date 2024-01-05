/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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
#include "territory_type.h"


using std::string;


string
territory_type_to_string(TerritoryType type)
{
	switch (type) {
	case TerritoryType::INTERNATIONAL_TEXTED:
		return "international-texted";
	case TerritoryType::INTERNATIONAL_TEXTLESS:
		return "international-textless";
	case TerritoryType::SPECIFIC:
		return "specific";
	}

	DCPOMATIC_ASSERT(false);
}


TerritoryType
string_to_territory_type(string type)
{
	if (type == "international-texted") {
		return TerritoryType::INTERNATIONAL_TEXTED;
	} else if (type == "international-textless") {
		return TerritoryType::INTERNATIONAL_TEXTLESS;
	} else if (type == "specific") {
		return TerritoryType::SPECIFIC;
	}

	DCPOMATIC_ASSERT(false);
}

