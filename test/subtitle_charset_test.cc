/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/string_text_file.h"
#include "lib/string_text_file_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::dynamic_pointer_cast;


/** Test parsing of UTF16 CR/LF input */
BOOST_AUTO_TEST_CASE (subtitle_charset_test1)
{
	auto content = content_factory (TestPaths::private_data() / "PADDINGTON soustitresVFdef.srt").front();
	auto film = new_test_film2 ("subtitle_charset_test1", { content });
}


/** Test parsing of OSX input */
BOOST_AUTO_TEST_CASE (subtitle_charset_test2)
{
	auto content = content_factory ("test/data/osx.srt").front();
	auto film = new_test_film2 ("subtitle_charset_test2", { content });
	auto ts = dynamic_pointer_cast<StringTextFileContent> (content);
	BOOST_REQUIRE (ts);
	/* Make sure we got the subtitle data from the file */
	BOOST_REQUIRE_EQUAL (content->full_length(film).get(), 6052032);
}
