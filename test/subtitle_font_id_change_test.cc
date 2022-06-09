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


/** @file  test/subtitle_font_id_change_test.cc
 *  @brief Check that old projects can still be used after the changes in 5a820bb8fae34591be5ac6d19a73461b9dab532a
 */


#include "lib/check_content_job.h"
#include "lib/content.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/verify.h>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::string;


class Editor
{
public:
	Editor (boost::filesystem::path path)
		: _path(path)
		, _content(dcp::file_to_string(path))
	{

	}

	~Editor ()
	{
		auto f = fopen(_path.string().c_str(), "w");
		BOOST_REQUIRE(f);
		fwrite(_content.c_str(), _content.length(), 1, f);
		fclose(f);
	}

	void replace (string a, string b)
	{
		auto old_content = _content;
		boost::algorithm::replace_all (_content, a, b);
		BOOST_REQUIRE (_content != old_content);
	}

private:
	boost::filesystem::path _path;
	std::string _content;
};


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test1)
{
	auto film = new_test_film2("subtitle_font_id_change_test1");
	boost::filesystem::remove(film->file("metadata.xml"));
	boost::filesystem::copy_file("test/data/subtitle_font_id_change_test1.xml", film->file("metadata.xml"));
	film->read_metadata();

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_REQUIRE_EQUAL(content[0]->text.size(), 1U);

	content[0]->set_paths({"test/data/short.srt"});

	CheckContentJob check(film);
	check.run();

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });
}


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test2)
{
	auto film = new_test_film2("subtitle_font_id_change_test2");
	boost::filesystem::remove(film->file("metadata.xml"));
	boost::filesystem::copy_file("test/data/subtitle_font_id_change_test2.xml", film->file("metadata.xml"));
	{
		Editor editor(film->file("metadata.xml"));
		editor.replace("/usr/share/fonts/truetype/inconsolata/Inconsolata.otf", "test/data/Inconsolata-VF.ttf");
	}
	film->read_metadata();

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_REQUIRE_EQUAL(content[0]->text.size(), 1U);

	content[0]->set_paths({"test/data/short.srt"});

	CheckContentJob check(film);
	check.run();

	auto font = content[0]->text.front()->get_font("");
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });
}


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test3)
{
	auto film = new_test_film2("subtitle_font_id_change_test3");
	boost::filesystem::remove(film->file("metadata.xml"));
	boost::filesystem::copy_file("test/data/subtitle_font_id_change_test3.xml", film->file("metadata.xml"));
	{
		Editor editor(film->file("metadata.xml"));
		editor.replace("/usr/share/fonts/truetype/inconsolata/Inconsolata.otf", "test/data/Inconsolata-VF.ttf");
	}
	film->read_metadata();

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_REQUIRE_EQUAL(content[0]->text.size(), 1U);

	content[0]->set_paths({"test/data/fonts.ass"});

	CheckContentJob check(film);
	check.run();

	auto font = content[0]->text.front()->get_font("Arial Black");
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	font = content[0]->text.front()->get_font("Helvetica Neue");
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });
}
