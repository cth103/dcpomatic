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


#include "lib/content_factory.h"
#include "lib/dcp_digest_file.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/dcp.h>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (dcp_digest_file_test)
{
	dcp::DCP dcp("test/data/dcp_digest_test_dcp");
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);

	write_dcp_digest_file ("build/test/digest.xml", dcp.cpls()[0], "e684e49e89182e907dabe5d9b3bd81ba");

	check_xml ("test/data/digest.xml", "build/test/digest.xml", {});
}

