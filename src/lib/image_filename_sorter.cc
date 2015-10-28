/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#include "raw_convert.h"

class ImageFilenameSorter
{
public:
	bool operator() (boost::filesystem::path a, boost::filesystem::path b)
	{
		std::list<int> na = extract_numbers (a);
		std::list<int> nb = extract_numbers (b);
		if (na.empty() || nb.empty()) {
			return a.string() < b.string();
		}

		if (na.size() != nb.size()) {
			/* Just use the first one */
			return na.front() < nb.front();
		}

		std::list<int>::const_iterator i = na.begin ();
		std::list<int>::const_iterator j = nb.begin ();

		while (i != na.end()) {
			if (*i != *j) {
				return *i < *j;
			}
			++i;
			++j;
		}

		/* All the same */
		return false;
	}

private:
	std::list<int> extract_numbers (boost::filesystem::path p)
	{
		p = p.leaf ();

		std::list<std::string> numbers;

		std::string current;
		for (size_t i = 0; i < p.string().size(); ++i) {
			if (isdigit (p.string()[i])) {
				current += p.string()[i];
			} else {
				if (!current.empty ()) {
					numbers.push_back (current);
					current.clear ();
				}
			}
		}

		if (!current.empty ()) {
			numbers.push_back (current);
		}

		std::list<int> numbers_as_int;
		BOOST_FOREACH (std::string i, numbers) {
			numbers_as_int.push_back (raw_convert<int> (i));
		}

		return numbers_as_int;
	}
};
