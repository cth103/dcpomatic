/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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

#include "image_filename_sorter.h"
#include <dcp/locale_convert.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <iostream>

using std::list;
using std::string;
using dcp::locale_convert;
using boost::optional;

bool
ImageFilenameSorter::operator() (boost::filesystem::path a, boost::filesystem::path b)
{
	string an = extract_numbers (a);
	string bn = extract_numbers (b);

	int const anl = an.length ();
	int const bnl = bn.length ();

	if (anl > bnl) {
		bn = string(anl - bnl, '0') + bn;
	} else if (bnl > anl) {
		an = string(bnl - anl, '0') + an;
	}

	return an < bn;
}

string
ImageFilenameSorter::extract_numbers (boost::filesystem::path p)
{
	string numbers;
	string const ps = p.leaf().string();
	for (size_t i = 0; i < ps.size(); ++i) {
		if (isdigit (ps[i])) {
			numbers += ps[i];
		}
	}
	return numbers;
}
