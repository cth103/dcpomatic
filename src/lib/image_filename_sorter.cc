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
#include <dcp/raw_convert.h>

class ImageFilenameSorter
{
public:
	bool operator() (boost::filesystem::path a, boost::filesystem::path b)
	{
		boost::optional<int> na = extract_number (a);
		boost::optional<int> nb = extract_number (b);
		if (!na || !nb) {
			std::cout << a << " " << b << " " << (a.string() < b.string()) << "\n";
			return a.string() < b.string();
		}

		return na.get() < nb.get();
	}

private:
	boost::optional<int> extract_number (boost::filesystem::path p)
	{
		p = p.leaf ();
		
		std::string number;
		for (size_t i = 0; i < p.string().size(); ++i) {
			if (isdigit (p.string()[i])) {
				number += p.string()[i];
			} else {
				if (!number.empty ()) {
					break;
				}
			}
		}

		if (number.empty ()) {
			return boost::optional<int> ();
		}

		return libdcp::raw_convert<int> (number);
	}
};
