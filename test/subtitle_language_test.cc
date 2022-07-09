/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/language_tag.h>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::string;
using std::vector;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (subtitle_language_interop_test)
{
	string const name = "subtitle_language_interop_test";
	auto fr = content_factory("test/data/frames.srt");
	auto film = new_test_film2 (name, fr);

	fr[0]->only_text()->set_language (dcp::LanguageTag("fr-FR"));
	film->set_interop (true);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION
		});

	check_dcp (String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()));
}


BOOST_AUTO_TEST_CASE (subtitle_language_smpte_test)
{
	string const name = "subtitle_language_smpte_test";
	auto fr = content_factory("test/data/frames.srt");
	auto film = new_test_film2 (name, fr);

	fr[0]->only_text()->set_language (dcp::LanguageTag("fr-FR"));
	film->set_interop (false);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	check_dcp (String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()));
}

