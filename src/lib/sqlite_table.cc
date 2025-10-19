/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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
#include "sqlite_table.h"
#include "util.h"


using std::string;
using std::vector;


void
SQLiteTable::add_column(string const& name, string const& type)
{
	_columns.push_back(name);
	_types.push_back(type);
}


string
SQLiteTable::create() const
{
	DCPOMATIC_ASSERT(!_columns.empty());
	DCPOMATIC_ASSERT(_columns.size() == _types.size());
	vector<string> columns(_columns.size());
	for (size_t i = 0; i < _columns.size(); ++i) {
		columns[i] = _columns[i] + " " + _types[i];
	}
	return fmt::format("CREATE TABLE IF NOT EXISTS {} (id INTEGER PRIMARY KEY, {})", _name, join_strings(columns, ", "));
}


string
SQLiteTable::insert() const
{
	DCPOMATIC_ASSERT(!_columns.empty());
	vector<string> placeholders(_columns.size(), "?");
	return fmt::format("INSERT INTO {} ({}) VALUES ({})", _name, join_strings(_columns, ", "), join_strings(placeholders, ", "));
}


string
SQLiteTable::update(string const& condition) const
{
	DCPOMATIC_ASSERT(!_columns.empty());
	vector<string> placeholders(_columns.size());
	for (size_t i = 0; i < _columns.size(); ++i) {
		placeholders[i] = _columns[i] + "=?";
	}

	return fmt::format("UPDATE {} SET {} {}", _name, join_strings(placeholders, ", "), condition);
}


string
SQLiteTable::select(string const& condition) const
{
	return fmt::format("SELECT id,{} FROM {} {}", join_strings(_columns, ","), _name, condition);
}


string
SQLiteTable::remove(string const& condition) const
{
	DCPOMATIC_ASSERT(!_columns.empty());
	return fmt::format("DELETE FROM {} {}", _name, condition);
}
