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


#ifndef DCPOMATIC_SHOW_PLAYLIST_ID
#define DCPOMATIC_SHOW_PLAYLIST_ID


#include "id.h"


/** @class ShowPlaylistID
 *
 *  @brief The SQLite ID (not the UUID) of a ShowPlaylist.
 */
class ShowPlaylistID : public ID
{
public:
	explicit ShowPlaylistID(sqlite3_int64 id)
		: ID(id) {}

	bool operator<(ShowPlaylistID const& other) const {
		return get() < other.get();
	}
};


#endif

