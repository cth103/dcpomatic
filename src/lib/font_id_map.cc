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
#include "font.h"
#include "font_id_map.h"


using std::shared_ptr;
using std::string;


std::string
FontIdMap::get(shared_ptr<dcpomatic::Font> font) const
{
	auto iter = _map.find(font);
	DCPOMATIC_ASSERT(iter != _map.end());
	return iter->second;
}


void
FontIdMap::put(shared_ptr<dcpomatic::Font> font, string id)
{
	_map[font] = id;
}

