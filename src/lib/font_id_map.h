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


#ifndef DCPOMATIC_FONT_ID_MAP_H
#define DCPOMATIC_FONT_ID_MAP_H


#include <map>
#include <string>


namespace dcpomatic {
	class Font;
}


class FontIdMap
{
public:
	std::string get(std::shared_ptr<dcpomatic::Font> font) const;
	void put(std::shared_ptr<dcpomatic::Font> font, std::string id);

	std::map<std::shared_ptr<dcpomatic::Font>, std::string> const& map() const {
		return _map;
	}

private:
	std::map<std::shared_ptr<dcpomatic::Font>, std::string> _map;

};


#endif

