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


#ifndef DCPOMATIC_SHOW_PLAYLIST_LIST_H
#define DCPOMATIC_SHOW_PLAYLIST_LIST_H


#include "show_playlist_entry.h"
#include "show_playlist_id.h"
#include "sqlite_database.h"
#include "sqlite_table.h"


class ShowPlaylist;
class SQLiteStatement;


/** @class ShowPlaylistList
 *
 *  @brief A list of SPLs (show playlists) stored in a SQLite database.
 *
 *  A SPL (show playlist) is a list of content (and maybe later automation cues)
 *  that make up a "show" in a cinema/theater.  For example, a SPL might contain
 *  some adverts, some trailers and a feature.
 *
 *  There are two tables: show_playlists and entries.  show_playlists contains
 *  just playlist UUIDs with their names.
 */
class ShowPlaylistList
{
public:
	ShowPlaylistList();
	explicit ShowPlaylistList(boost::filesystem::path db_file);

	/** Write a ShowPlaylist to the database, returning its new SQLite ID */
	ShowPlaylistID add_show_playlist(ShowPlaylist const& show_playlist);
	void update_show_playlist(ShowPlaylistID id, ShowPlaylist const& show_playlist);
	void remove_show_playlist(ShowPlaylistID id);

	std::vector<std::pair<ShowPlaylistID, ShowPlaylist>> show_playlists() const;
	boost::optional<ShowPlaylist> show_playlist(ShowPlaylistID id) const;

	boost::optional<ShowPlaylistID> get_show_playlist_id(std::string const& show_playlist_uuid) const;

	/** @return The entries on a given show playlist, given the playlist's SQLite ID */
	std::vector<ShowPlaylistEntry> entries(ShowPlaylistID show_playlist_id) const;
	/** @return The entries on a given show playlist, given the playlist's UUID */
	std::vector<ShowPlaylistEntry> entries(std::string const& show_playlist_uuid) const;

	bool missing(std::string const& show_playlist_uuid) const;
	bool missing(ShowPlaylistID id) const;

	/** Add a playlist entry to the end of a playlist in the database */
	void add_entry(ShowPlaylistID, ShowPlaylistEntry const& entry);
	/** Set the values in the database from entry */
	void update_entry(ShowPlaylistID, int index, ShowPlaylistEntry const& entry);
	/** Remove a playlist entry from the database */
	void remove_entry(ShowPlaylistID, int index);
	/** Move the given playlist entry one place higher (earlier) */
	void move_entry_up(ShowPlaylistID, int index);
	/** Move the given playlist entry one place lower (later) */
	void move_entry_down(ShowPlaylistID, int index);

	void read_legacy(boost::filesystem::path dir);

private:
	void setup_tables();
	void setup();
	std::vector<ShowPlaylistEntry> entries(std::string const& where, std::function<void (SQLiteStatement&)> bind) const;
	void swap_entries(ShowPlaylistID playlist_id, int index);

	SQLiteTable _show_playlists;
	SQLiteTable _entries;
	mutable SQLiteDatabase _db;
};



#endif
