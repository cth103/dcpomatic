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


#include "exceptions.h"
#include "sqlite_database.h"
#include <sqlite3.h>


SQLiteDatabase::SQLiteDatabase(boost::filesystem::path path)
{
#ifdef DCPOMATIC_WINDOWS
	auto rc = sqlite3_open16(path.c_str(), &_db);
#else
	auto rc = sqlite3_open(path.c_str(), &_db);
#endif
	if (rc != SQLITE_OK) {
		throw FileError("Could not open SQLite database", path);
	}

	sqlite3_busy_timeout(_db, 500);
}


SQLiteDatabase::SQLiteDatabase(SQLiteDatabase&& other)
{
	_db = other._db;
	other._db = nullptr;
}


SQLiteDatabase&
SQLiteDatabase::operator=(SQLiteDatabase&& other)
{
	if (this != &other) {
		_db = other._db;
		other._db = nullptr;
	}
	return *this;
}


SQLiteDatabase::~SQLiteDatabase()
{
	if (_db) {
		sqlite3_close(_db);
	}
}


