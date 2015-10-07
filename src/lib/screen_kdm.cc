/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "screen_kdm.h"
#include "cinema.h"
#include "screen.h"
#include "util.h"
#include <boost/foreach.hpp>

using std::string;
using std::list;
using boost::shared_ptr;

bool
operator== (ScreenKDM const & a, ScreenKDM const & b)
{
	return a.screen == b.screen && a.kdm == b.kdm;
}

/** @param first_part first part of the filename (perhaps the name of the film) */
string
ScreenKDM::filename (string first_part) const
{
	return tidy_for_filename (first_part) + "_" + tidy_for_filename (screen->cinema->name) + "_" + tidy_for_filename (screen->name) + ".kdm.xml";
}

void
ScreenKDM::write_files (string name_first_part, list<ScreenKDM> screen_kdms, boost::filesystem::path directory)
{
	/* Write KDMs to the specified directory */
	BOOST_FOREACH (ScreenKDM const & i, screen_kdms) {
		boost::filesystem::path out = directory / i.filename(name_first_part);
		i.kdm.as_xml (out);
	}
}
