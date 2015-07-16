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

#include "raw_convert.h"
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <iostream>

class ImageFilenameSorter
{
public:
	bool operator() (boost::filesystem::path a, boost::filesystem::path b)
	{
		std::vector<int> na = extract_numbers (a);
		std::vector<int> nb = extract_numbers (b);

		std::vector<int>::const_iterator i = na.begin ();
		std::vector<int>::const_iterator j = nb.begin ();

		while (true) {
			if (i == na.end () || j == nb.end ()) {
				return false;
			}

			if (*i != *j) {
				return *i < *j;
			}

			++i;
			++j;
		}

		/* NOT REACHED */
		return false;
	}

private:
	std::vector<int> extract_numbers (boost::filesystem::path p)
	{
		p = p.leaf ();

		std::vector<int> numbers;
		std::string number;
		for (size_t i = 0; i < p.string().size(); ++i) {
			if (isdigit (p.string()[i])) {
				number += p.string()[i];
			} else if (!number.empty ()) {
				numbers.push_back (raw_convert<int> (number));
				number.clear ();
			}
		}

		if (!number.empty ()) {
			numbers.push_back (raw_convert<int> (number));
		}

		return numbers;
	}
};
