/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_COLLATOR_H
#define DCPOMATIC_COLLATOR_H


#include <string>


struct UCollator;


class Collator
{
public:
	Collator(char const* locale = nullptr);
	~Collator();

	Collator(Collator const &) = delete;
	Collator& operator=(Collator const&) = delete;

	int compare(std::string const& utf8_a, std::string const& utf8_b) const;
	bool find(std::string pattern, std::string text) const;

private:
	UCollator* _collator = nullptr;
};


#endif

