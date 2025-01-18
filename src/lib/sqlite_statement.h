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


#include <sqlite3.h>
#include <functional>
#include <string>


class SQLiteDatabase;


class SQLiteStatement
{
public:
	SQLiteStatement(SQLiteDatabase& db, std::string const& statement);
	~SQLiteStatement();

	SQLiteStatement(SQLiteStatement const&) = delete;
	SQLiteStatement& operator=(SQLiteStatement const&) = delete;

	void bind_text(int index, std::string const& value);
	void bind_int64(int index, int64_t value);

	int64_t column_int64(int index);
	std::string column_text(int index);

	void execute(std::function<void(SQLiteStatement&)> row = std::function<void(SQLiteStatement& statement)>(), std::function<void()> busy = std::function<void()>());

	int data_count();

private:
	SQLiteDatabase& _db;
	sqlite3_stmt* _stmt;
};

