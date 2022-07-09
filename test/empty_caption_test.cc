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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (check_for_no_empty_text_nodes_in_failure_case)
{
	auto content = content_factory("test/data/empty.srt");
	auto film = new_test_film2 ("check_for_no_empty_text_nodes_in_failure_case", content);
	auto text = content[0]->text.front();
	text->set_type (TextType::CLOSED_CAPTION);
	text->set_dcp_track({"English", dcp::LanguageTag("en-GB")});

	make_and_verify_dcp (film, {
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
}

