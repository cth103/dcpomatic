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

/** @file  test/subtitle_language_test.cc
 *  @brief Test that subtitle language information is correctly written to DCPs.
 */


#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/language_tag.h>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::string;
using std::vector;
using boost::shared_ptr;


BOOST_AUTO_TEST_CASE (subtitle_language_interop_test)
{
	string const name = "subtitle_language_interop_test";
	shared_ptr<Film> film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/frames.srt").front());
	BOOST_REQUIRE (!wait_for_jobs());

	vector<dcp::LanguageTag> langs;
	langs.push_back(dcp::LanguageTag("fr-FR"));
	langs.push_back(dcp::LanguageTag("de-DE"));
	film->set_subtitle_languages(langs);
	film->set_interop (true);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	check_dcp (String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()));
}


BOOST_AUTO_TEST_CASE (subtitle_language_smpte_test)
{
	string const name = "subtitle_language_smpte_test";
	shared_ptr<Film> film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/frames.srt").front());
	BOOST_REQUIRE (!wait_for_jobs());

	vector<dcp::LanguageTag> langs;
	langs.push_back(dcp::LanguageTag("fr-FR"));
	langs.push_back(dcp::LanguageTag("de-DE"));
	film->set_subtitle_languages(langs);
	film->set_interop (false);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	check_dcp (String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()));
}

