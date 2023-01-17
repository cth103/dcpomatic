/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/collator.h"
#include <unicode/uenum.h>
#include <unicode/coll.h>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(collator_compare_works_and_ignores_case)
{
	Collator collator("en");

#if 0
	// Print out available locales
	// UErrorCode status;
	// auto available = ucol_openAvailableLocales(&status);
	// int32_t length = 0;
	// while (true) {
	// 	auto next = uenum_next(available, &length, &status);
	// 	if (!next) {
	// 		break;
	// 	}
	// 	std::cout << next << "\n";
	// }
#endif

	BOOST_CHECK_EQUAL(collator.compare("So often YOU won't even notice", "SO OFTEN you won't even NOTiCE"), 0);
	BOOST_CHECK_EQUAL(collator.compare("So often YOU won't even notice", "SO OFTEN you won't even see"), -1);
}



BOOST_AUTO_TEST_CASE(collator_search_works_and_ignores_case)
{
	Collator collator("en");

	BOOST_CHECK(collator.find("outh", "With filthy mouths, and bad attitudes"));
	BOOST_CHECK(collator.find("with", "With filthy mouths, and bad attitudes"));
	BOOST_CHECK(!collator.find("ostrabagalous", "With filthy mouths, and bad attitudes"));
}
