/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "state.h"
#include <glib.h>

using std::string;

boost::optional<boost::filesystem::path> State::override_path;

/** @param file State filename
 *  @return Full path to write @file to */
boost::filesystem::path
State::path (string file, bool create_directories)
{
	boost::filesystem::path p;
	if (override_path) {
		p = *override_path;
	} else {
#ifdef DCPOMATIC_OSX
		p /= g_get_home_dir ();
		p /= "Library";
		p /= "Preferences";
		p /= "com.dcpomatic";
		p /= "2";
#else
		p /= g_get_user_config_dir ();
		p /= "dcpomatic2";
#endif
	}
	boost::system::error_code ec;
	if (create_directories) {
		boost::filesystem::create_directories (p, ec);
	}
	p /= file;
	return p;
}
