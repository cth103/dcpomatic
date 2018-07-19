/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/caption_content.h"
#include "lib/text_caption_file_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

/** Basic test that Interop closed captions are written */
BOOST_AUTO_TEST_CASE (closed_caption_test1)
{
	shared_ptr<Film> film = new_test_film2 ("closed_caption_test1");
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/subrip.srt"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	content->caption->set_type (CAPTION_CLOSED);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Just check to see that there's a CCAP in the CPL: this
	   check could be better!
	*/

	dcp::DCP check (film->dir(film->dcp_name()));
	check.read ();

	BOOST_REQUIRE_EQUAL (check.cpls().size(), 1);
	BOOST_REQUIRE_EQUAL (check.cpls().front()->reels().size(), 1);
	BOOST_REQUIRE (check.cpls().front()->reels().front()->closed_caption());
}
