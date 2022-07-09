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


#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


/* Sanity check to make sure that files in a DCP have the right extensions / names.
 * This is mostly to catch a crazy mistake where Interop subtitle files suddenly got
 * a MXF extension but no tests caught it (#2270).
 */
BOOST_AUTO_TEST_CASE (interop_file_extension_test)
{
	auto video = content_factory("test/data/flat_red.png")[0];
	auto audio = content_factory("test/data/sine_440.wav")[0];
	auto sub = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film2("interop_file_extension_test", { video, audio, sub });
	film->set_interop(true);

	make_and_verify_dcp(
		film, {
			    dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			    dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			    dcp::VerificationNote::Code::INVALID_STANDARD
			});

	BOOST_REQUIRE(dcp_file(film, "ASSETMAP").extension() == "");
	BOOST_REQUIRE(dcp_file(film, "VOLINDEX").extension() == "");
	BOOST_REQUIRE(dcp_file(film, "cpl").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "pkl").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "j2c").extension() == ".mxf");
	BOOST_REQUIRE(dcp_file(film, "pcm").extension() == ".mxf");
	BOOST_REQUIRE(dcp_file(film, "sub").extension() == ".xml");
}


BOOST_AUTO_TEST_CASE (smpte_file_extension_test)
{
	auto video = content_factory("test/data/flat_red.png")[0];
	auto audio = content_factory("test/data/sine_440.wav")[0];
	auto sub = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film2("smpte_file_extension_test", { video, audio, sub });
	film->set_interop(false);

	make_and_verify_dcp(
		film, {
			    dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			    dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE
			});

	BOOST_REQUIRE(dcp_file(film, "ASSETMAP").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "VOLINDEX").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "cpl").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "pkl").extension() == ".xml");
	BOOST_REQUIRE(dcp_file(film, "j2c").extension() == ".mxf");
	BOOST_REQUIRE(dcp_file(film, "pcm").extension() == ".mxf");
	BOOST_REQUIRE(dcp_file(film, "sub").extension() == ".mxf");
}
