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


#ifndef DCPOMATIC_SQLITE_TABLE_H
#define DCPOMATIC_SQLITE_TABLE_H

#include <string>
#include <vector>


class SQLiteTable
{
public:
	SQLiteTable(std::string name)
		: _name(std::move(name))
	{}

	SQLiteTable(SQLiteTable const&) = default;
	SQLiteTable(SQLiteTable&&) = default;

	void add_column(std::string const& name, std::string const& type);

	std::string create() const;
	std::string insert() const;
	std::string update(std::string const& condition) const;
	std::string select(std::string const& condition) const;
	std::string remove(std::string const& condition) const;

private:
	std::string _name;
	std::vector<std::string> _columns;
	std::vector<std::string> _types;
};


#endif

