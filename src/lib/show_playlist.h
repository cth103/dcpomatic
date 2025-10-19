/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_SHOW_PLAYLIST_H
#define DCPOMATIC_SHOW_PLAYLIST_H


#include <dcp/util.h>
#include <nlohmann/json.hpp>


/** @class ShowPlaylist
 *
 *  @brief A "show playlist": what a projection system might play for an entire cinema "show".
 *
 *  For example, it might contain some adverts, some trailers and a feature.
 *  Each SPL has unique ID, a name, and some ordered entries (the content).  The content
 *  is not stored in this class, but can be read from the database by ShowPlaylistList.
 */
class ShowPlaylist
{
public:
	ShowPlaylist()
		: _uuid(dcp::make_uuid())
	{}

	explicit ShowPlaylist(std::string name)
		: _uuid(dcp::make_uuid())
		, _name(name)
	{}

	ShowPlaylist(std::string uuid, std::string name)
		: _uuid(uuid)
		, _name(name)
	{}

	std::string uuid() const {
		return _uuid;
	}

	std::string name() const {
		return _name;
	}

	void set_name(std::string name) {
		_name = name;
	}

	nlohmann::json as_json() const;

private:
	std::string _uuid;
	std::string _name;
};


bool operator==(ShowPlaylist const& a, ShowPlaylist const& b);
bool operator!=(ShowPlaylist const& a, ShowPlaylist const& b);


#endif
