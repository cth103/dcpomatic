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


#include "lib/config.h"
#include "lib/release_notes.h"
#include <boost/test/unit_test.hpp>


using std::string;


// Once we're running 2.17.19 we have no more release notes (for now, at least)
BOOST_AUTO_TEST_CASE(release_notes_test2)
{
	for (auto version: { "2.17.19", "2.17.20", "2.18.0", "2.18.1devel6" }) {
		Config::instance()->set_last_release_notes_version("2.17.19");
		auto notes = find_release_notes(false, string(version));
		BOOST_CHECK(!static_cast<bool>(notes));
	}
}
