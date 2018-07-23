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

#include "test.h"
#include "lib/content.h"
#include "lib/film.h"
#include "lib/content_factory.h"
#include "lib/string_text_file.h"
#include "lib/string_text_file_content.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Test parsing of UTF16 CR/LF input */
BOOST_AUTO_TEST_CASE (subtitle_charset_test1)
{
	shared_ptr<Film> film = new_test_film2 ("subtitle_charset_test1");
	shared_ptr<Content> content = content_factory (film, private_data / "PADDINGTON soustitresVFdef.srt").front ();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Test parsing of OSX input */
BOOST_AUTO_TEST_CASE (subtitle_charset_test2)
{
	shared_ptr<Film> film = new_test_film2 ("subtitle_charset_test2");
	shared_ptr<Content> content = content_factory (film, "test/data/osx.srt").front ();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	shared_ptr<StringTextFileContent> ts = dynamic_pointer_cast<StringTextFileContent> (content);
	BOOST_REQUIRE (ts);
	/* Make sure we got the subtitle data from the file */
	BOOST_REQUIRE_EQUAL (content->full_length().get(), 6052032);
}
