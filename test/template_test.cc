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


#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::string;


/* Bug #2491 */
BOOST_AUTO_TEST_CASE(template_wrong_channel_counts)
{
	ConfigRestorer cr("test/data");

	auto film = new_test_film("template_wrong_channel_counts", {});
	film->use_template(string("Bug"));

	auto mono = content_factory("test/data/C.wav").front();
	film->examine_and_add_content({mono});
	BOOST_REQUIRE(!wait_for_jobs());

	BOOST_REQUIRE_EQUAL(mono->audio->streams().size(), 1U);
	BOOST_CHECK_EQUAL(mono->audio->streams()[0]->channels(), 1);
}

