/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/cross.h"
#include "lib/util.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>


BOOST_AUTO_TEST_CASE (fix_long_path_test)
{
#ifdef DCPOMATIC_WINDOWS
	BOOST_CHECK_EQUAL (fix_long_path("c:\\foo"), "\\\\?\\c:\\foo");
	BOOST_CHECK_EQUAL (fix_long_path("c:\\foo\\bar"), "\\\\?\\c:\\foo\\bar");
	boost::filesystem::path fixed_bar = "\\\\?\\";
	fixed_bar += boost::filesystem::current_path();
	fixed_bar /= "bar";
	BOOST_CHECK_EQUAL (fix_long_path("bar"), fixed_bar);
#else
	BOOST_CHECK_EQUAL (fix_long_path("foo/bar/baz"), "foo/bar/baz");
#endif
}


#ifdef DCPOMATIC_WINDOWS
BOOST_AUTO_TEST_CASE (windows_long_filename_test)
{
	using namespace boost::filesystem;

	path too_long = current_path() / "build\\test\\a\\really\\very\\long\\filesystem\\path\\indeed\\that\\will\\be\\so\\long\\that\\windows\\cannot\\normally\\cope\\with\\it\\unless\\we\\add\\this\\crazy\\prefix\\and\\then\\magically\\it\\can\\do\\it\\fine\\I\\dont\\really\\know\\why\\its\\like\\that\\but\\hey\\it\\is\\so\\here\\we\\are\\what\\can\\we\\do\\other\\than\\bodge\\it";

	BOOST_CHECK (too_long.string().length() > 260);
	boost::system::error_code ec;
	create_directories (too_long, ec);
	BOOST_CHECK (ec);

	path fixed_path = fix_long_path(too_long);
	create_directories (fixed_path, ec);
	BOOST_CHECK (!ec);

	auto file = fopen_boost(too_long / "hello", "w");
	BOOST_REQUIRE (file);
	fprintf (file, "Hello_world");
	fclose (file);

	file = fopen_boost(too_long / "hello", "r");
	BOOST_REQUIRE (file);
	char buffer[64];
	fscanf (file, "%63s", buffer);
	BOOST_CHECK_EQUAL (strcmp(buffer, "Hello_world"), 0);
}
#endif

