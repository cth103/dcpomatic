/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/gpu_util.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


#ifdef DCPOMATIC_GROK
BOOST_AUTO_TEST_CASE(get_gpu_names_test)
{
	auto names = get_gpu_names("test/gpu_lister", "build/test/gpus.txt");
	BOOST_REQUIRE_EQUAL(names.size(), 3U);
	BOOST_CHECK_EQUAL(names[0], "Foo bar baz");
	BOOST_CHECK_EQUAL(names[1], "Spondoolix Mega Kompute 2000");
	BOOST_CHECK_EQUAL(names[2], "Energy Sink-o-matic");
}
#endif
