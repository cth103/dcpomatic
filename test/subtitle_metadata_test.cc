/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

/** @file  test/subtitle_metadata_test.cc
 *  @brief Test that subtitle language metadata is recovered from metadata files
 *  written by versions before the subtitle language was only stored in Film.
 */


#include "lib/film.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>


using std::vector;
using boost::shared_ptr;


BOOST_AUTO_TEST_CASE (subtitle_metadata_test1)
{
	using namespace boost::filesystem;

	path p = test_film_dir ("subtitle_metadata_test1");
	if (exists (p)) {
		remove_all (p);
	}
	create_directory (p);

	copy_file ("test/data/subtitle_metadata1.xml", p / "metadata.xml");
	shared_ptr<Film> film(new Film(p));
	film->read_metadata();

	vector<dcp::LanguageTag> langs = film->subtitle_languages ();
	BOOST_REQUIRE (!langs.empty());
	BOOST_CHECK_EQUAL (langs.front().to_string(), "de-DE");
}


BOOST_AUTO_TEST_CASE (subtitle_metadata_test2)
{
	using namespace boost::filesystem;

	path p = test_film_dir ("subtitle_metadata_test2");
	if (exists (p)) {
		remove_all (p);
	}
	create_directory (p);

	copy_file ("test/data/subtitle_metadata2.xml", p / "metadata.xml");
	shared_ptr<Film> film(new Film(p));
	film->read_metadata();

	vector<dcp::LanguageTag> langs = film->subtitle_languages ();
	BOOST_REQUIRE (!langs.empty());
	BOOST_CHECK_EQUAL (langs.front().to_string(), "fr");
}

