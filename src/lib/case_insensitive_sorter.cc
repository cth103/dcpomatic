/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "case_insensitive_sorter.h"
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;

bool
CaseInsensitiveSorter::operator() (boost::filesystem::path a, boost::filesystem::path b)
{
	string x = a.string ();
	string y = b.string ();
	transform (x.begin(), x.end(), x.begin(), ::tolower);
	transform (y.begin(), y.end(), y.begin(), ::tolower);
	return x < y;
}
