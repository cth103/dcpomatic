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


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE (smpte_dcp_with_subtitles_can_be_decrypted)
{
	auto content = content_factory("test/data/15s.srt").front();
	auto film = new_test_film2 ("smpte_dcp_with_subtitles_can_be_decrypted", { content });
	film->set_interop (false);
	film->set_encrypted (true);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME,
		});

	dcp::DCP dcp (film->dir(film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE (cpl->file());

	auto kdm = film->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		{},
		*cpl->file(),
		dcp::LocalTime(),
		dcp::LocalTime(),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		true,
		0
		);

	auto dcp_content = make_shared<DCPContent>(film->dir(film->dcp_name()));
	dcp_content->add_kdm (kdm);
	DCPExaminer examiner (dcp_content, false);
	BOOST_CHECK (examiner.kdm_valid());
}

