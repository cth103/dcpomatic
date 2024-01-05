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


#ifndef DCPOMATIC_TERRITORY_TYPE_H
#define DCPOMATIC_TERRITORY_TYPE_H


#include <string>


enum class TerritoryType
{
	INTERNATIONAL_TEXTED,
	INTERNATIONAL_TEXTLESS,
	SPECIFIC
};


std::string territory_type_to_string(TerritoryType);
TerritoryType string_to_territory_type(std::string);


#endif

