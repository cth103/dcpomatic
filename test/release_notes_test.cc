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


// If we have no previous version we're on something before 2.16.19 so we want the release notes
BOOST_AUTO_TEST_CASE(release_notes_test1)
{
	for (auto version: { "2.16.19", "2.16.20", "2.18.0", "2.18.1devel6" }) {
		Config::instance()->unset_last_release_notes_version();
		auto notes = find_release_notes(string(version));
		BOOST_CHECK(notes.get_value_or("").find("In this version there are changes to the way that subtitles are positioned.") != string::npos);
	}
}


// Once we're running 2.16.19 we have no more release notes (for now, at least)
BOOST_AUTO_TEST_CASE(release_notes_test2)
{
	for (auto version: { "2.16.19", "2.16.20", "2.18.0", "2.18.1devel6" }) {
		Config::instance()->set_last_release_notes_version("2.16.19");
		auto notes = find_release_notes(string(version));
		BOOST_CHECK(!static_cast<bool>(notes));
	}
}
