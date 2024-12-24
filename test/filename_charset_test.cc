/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "lib/dcp_content.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


/* Check that a DCP can be imported from a non-ASCII path */
BOOST_AUTO_TEST_CASE(utf8_filename_handling_test)
{
	auto const dir = boost::filesystem::path("build/test/utf8_filename_handling_test_input/ᴟᶒḃↈ");
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);

	for (auto i: boost::filesystem::directory_iterator("test/data/dcp_digest_test_dcp")) {
		boost::filesystem::copy_file(i, dir / i.path().filename());
	}

	auto content = std::make_shared<DCPContent>(dir);
	auto film = new_test_film("utf8_filename_handling_test", { content });
}

