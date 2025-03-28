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


#include "lib/content_factory.h"
#include "lib/copy_dcp_details_to_film.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/language_tag.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(copy_audio_language_to_film)
{
	auto content = content_factory("test/data/sine_440.wav")[0];
	auto film1 = new_test_film("copy_audio_language_to_film1", { content });
	film1->set_audio_language(dcp::LanguageTag("de-DE"));
	make_and_verify_dcp(
		film1,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	auto dcp = make_shared<DCPContent>(film1->dir(film1->dcp_name()));
	auto film2 = new_test_film("copy_audio_language_to_film2", { dcp });
	copy_dcp_settings_to_film(dcp, film2);

	BOOST_REQUIRE(film2->audio_language());
	BOOST_CHECK_EQUAL(film2->audio_language()->as_string(), "de-DE");
}

