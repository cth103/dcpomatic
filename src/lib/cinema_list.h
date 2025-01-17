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


#ifndef DCPOMATIC_CINEMA_LIST_H
#define DCPOMATIC_CINEMA_LIST_H


#include "id.h"
#include "sqlite_table.h"
#include <libcxml/cxml.h>
#include <dcp/utc_offset.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <sqlite3.h>
#include <set>


class Cinema;
namespace dcpomatic {
	class Screen;
}
class SQLiteStatement;


class CinemaID : public ID
{
public:
	CinemaID(sqlite3_int64 id)
		: ID(id) {}

	bool operator<(CinemaID const& other) const {
		return get() < other.get();
	}
};


class ScreenID : public ID
{
public:
	ScreenID(sqlite3_int64 id)
		: ID(id) {}

	bool operator==(ScreenID const& other) const {
		return get() == other.get();
	}

	bool operator!=(ScreenID const& other) const {
		return get() != other.get();
	}

	bool operator<(ScreenID const& other) const {
		return get() < other.get();
	}
};


class CinemaList
{
public:
	CinemaList();
	CinemaList(boost::filesystem::path db_file);
	~CinemaList();

	CinemaList(CinemaList const&) = delete;
	CinemaList& operator=(CinemaList const&) = delete;

	CinemaList(CinemaList&& other);
	CinemaList& operator=(CinemaList&& other);

	void read_legacy_file(boost::filesystem::path xml_file);
	void read_legacy_string(std::string const& xml);

	void clear();

	CinemaID add_cinema(Cinema const& cinema);
	void update_cinema(CinemaID id, Cinema const& cinema);
	void remove_cinema(CinemaID id);
	std::vector<std::pair<CinemaID, Cinema>> cinemas() const;
	boost::optional<Cinema> cinema(CinemaID id) const;
	boost::optional<std::pair<CinemaID, Cinema>> cinema_by_partial_name(std::string const& text) const;
	boost::optional<std::pair<CinemaID, Cinema>> cinema_by_name_or_email(std::string const& text) const;

	ScreenID add_screen(CinemaID cinema_id, dcpomatic::Screen const& screen);
	void update_screen(CinemaID cinema_id, ScreenID screen_id, dcpomatic::Screen const& screen);
	void remove_screen(ScreenID id);
	boost::optional<dcpomatic::Screen> screen(ScreenID screen_id) const;
	std::vector<std::pair<ScreenID, dcpomatic::Screen>> screens(CinemaID cinema_id) const;
	std::vector<std::pair<ScreenID, dcpomatic::Screen>> screens_by_cinema_and_name(CinemaID id, std::string const& name) const;

	/** Call a callback with every screen.
	 *  @param with_trusted_devices true to read screen's trusted devices into the Screen, false to not bother.
	 */
	void screens(std::function<void (CinemaID, ScreenID, dcpomatic::Screen const&)> callback, bool with_trusted_devices) const;

	boost::optional<dcp::UTCOffset> unique_utc_offset(std::set<CinemaID> const& cinemas);

private:
	/** @param with_trusted_devices true to read screen's trusted devices into the Screen, false to not bother */
	dcpomatic::Screen screen_from_result(SQLiteStatement& statement, ScreenID screen_id, bool with_trusted_devices) const;
	std::vector<std::pair<ScreenID, dcpomatic::Screen>> screens_from_result(SQLiteStatement& statement) const;
	void setup_tables();
	void setup(boost::filesystem::path db_file);
	void read_legacy_document(cxml::Document const& doc);

	sqlite3* _db = nullptr;
	SQLiteTable _cinemas;
	SQLiteTable _screens;
	SQLiteTable _trusted_devices;
};



#endif

