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


#include "lib/config.h"
#include "lib/export_decryption_settings.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(test_export_decryption_settings)
{
	export_decryption_chain_and_key(Config::instance()->decryption_chain(), "build/test/foo.dom");
	auto test = import_decryption_chain_and_key("build/test/foo.dom");

	BOOST_REQUIRE(Config::instance()->decryption_chain()->root_to_leaf() == test->root_to_leaf());
	BOOST_REQUIRE(Config::instance()->decryption_chain()->key() == test->key());
}


BOOST_AUTO_TEST_CASE(test_import_pkcs8_settings)
{
	BOOST_CHECK(import_decryption_chain_and_key("test/data/pkcs8_state.dom"));
}

