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


#include "exceptions.h"
#include "sqlite_database.h"
#include "sqlite_statement.h"


using std::function;
using std::string;


SQLiteStatement::SQLiteStatement(SQLiteDatabase& db, string const& statement)
	: _db(db)
{
#ifdef DCPOMATIC_HAVE_SQLITE3_PREPARE_V3
	auto rc = sqlite3_prepare_v3(_db.db(), statement.c_str(), -1, 0, &_stmt, nullptr);
#else
	auto rc = sqlite3_prepare_v2(_db.db(), statement.c_str(), -1, &_stmt, nullptr);
#endif
	if (rc != SQLITE_OK) {
		throw SQLError(_db, rc, statement);
	}
}


SQLiteStatement::~SQLiteStatement()
{
	sqlite3_finalize(_stmt);
}


void
SQLiteStatement::bind_text(int index, string const& value)
{
	auto rc = sqlite3_bind_text(_stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		throw SQLError(_db, rc);
	}
}


void
SQLiteStatement::bind_int64(int index, int64_t value)
{
	auto rc = sqlite3_bind_int64(_stmt, index, value);
	if (rc != SQLITE_OK) {
		throw SQLError(_db, rc);
	}
}


void
SQLiteStatement::execute(function<void(SQLiteStatement&)> row, function<void()> busy)
{
	while (true) {
		auto const rc = sqlite3_step(_stmt);
		switch (rc) {
		case SQLITE_BUSY:
			busy();
			break;
		case SQLITE_DONE:
			return;
		case SQLITE_ROW:
			row(*this);
			break;
		case SQLITE_ERROR:
		case SQLITE_MISUSE:
			throw SQLError(_db, sqlite3_errmsg(_db.db()));
		}
	}
}


int
SQLiteStatement::data_count()
{
	return sqlite3_data_count(_stmt);
}


int64_t
SQLiteStatement::column_int64(int index)
{
	return sqlite3_column_int64(_stmt, index);
}


string
SQLiteStatement::column_text(int index)
{
	return reinterpret_cast<const char*>(sqlite3_column_text(_stmt, index));
}

