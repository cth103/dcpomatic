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


#include "lib/scoped_temporary.h"
#include <boost/test/unit_test.hpp>


using std::string;


BOOST_AUTO_TEST_CASE(scoped_temporary_path_allocated_on_construction)
{
	ScopedTemporary st;

	BOOST_REQUIRE(!st.path().empty());
}


BOOST_AUTO_TEST_CASE(scoped_temporary_write_read)
{
	ScopedTemporary st;

	auto& write = st.open("w");
	write.puts("hello world");
	auto& read = st.open("r");

	char buffer[64];
	read.gets(buffer, 64);
	BOOST_CHECK(string(buffer) == "hello world");
}


BOOST_AUTO_TEST_CASE(scoped_temporary_take)
{
	ScopedTemporary st;

	auto& got = st.open("w");
	auto taken = got.take();

	fclose(taken);
}
