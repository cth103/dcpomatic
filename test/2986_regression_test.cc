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
#include "lib/film.h"
#include "lib/find_missing.h"
#include "lib/font.h"
#include "lib/player.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(font_error_after_find_missing_test)
{
	boost::filesystem::path dir("build/test/font_error_after_find_missing_test_assets");
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);

	boost::filesystem::copy_file("test/data/15s.srt", dir / "15s.srt");
	boost::filesystem::copy_file("test/data/Inconsolata-VF.ttf", dir / "Inconsolata-VF.ttf");

	{
		auto content = content_factory(dir / "15s.srt");
		auto film = new_test_film("font_error_after_find_missing_test", content);
		auto fonts = content[0]->text[0]->fonts();
		fonts.front()->set_file(dir / "Inconsolata-VF.ttf");
		film->write_metadata();
	}

	boost::filesystem::remove_all(dir);

	auto film2 = make_shared<Film>(boost::filesystem::path("build/test/font_error_after_find_missing_test"));
	film2->read_metadata();

	dcpomatic::find_missing(film2->content(), "test/data/15s.srt");

	Player player(film2, Image::Alignment::PADDED, false);
	player.set_always_burn_open_subtitles();
	for (int i = 0; i < 48; ++i) {
		player.pass();
	}
}
