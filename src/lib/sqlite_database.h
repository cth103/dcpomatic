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


#ifndef DCPOMATIC_SQLITE_DATABASE_H
#define DCPOMATIC_SQLITE_DATABASE_H


#include <boost/filesystem.hpp>

struct sqlite3;


class SQLiteDatabase
{
public:
	SQLiteDatabase(boost::filesystem::path path);
	~SQLiteDatabase();

	SQLiteDatabase(SQLiteDatabase const&) = delete;
	SQLiteDatabase& operator=(SQLiteDatabase const&) = delete;

	SQLiteDatabase(SQLiteDatabase&&);
	SQLiteDatabase& operator=(SQLiteDatabase&& other);

	sqlite3* db() const {
		return _db;
	}

private:
	sqlite3* _db = nullptr;
};


#endif
