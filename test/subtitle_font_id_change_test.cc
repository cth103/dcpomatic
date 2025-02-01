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


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test1)
{
	auto film = new_test_film("subtitle_font_id_change_test1");
	boost::filesystem::remove(film->file("metadata.xml"));
	boost::filesystem::copy_file("test/data/subtitle_font_id_change_test1.xml", film->file("metadata.xml"));
	film->read_metadata();

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_REQUIRE_EQUAL(content[0]->text.size(), 1U);

	content[0]->set_paths({"test/data/short.srt"});
	content[0]->only_text()->set_language(dcp::LanguageTag("de"));

	CheckContentJob check(film);
	check.run();
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });
}


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test2)
{
	auto film = new_test_film("subtitle_font_id_change_test2");
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
	/* Make sure the content doesn't look like it's changed, otherwise it will be re-examined
	 * which obscures the point of this test.
	 */
	content[0]->_last_write_times[0] = boost::filesystem::last_write_time("test/data/short.srt");
	content[0]->only_text()->set_language(dcp::LanguageTag("de"));

	CheckContentJob check(film);
	check.run();
	BOOST_REQUIRE (!wait_for_jobs());

	auto font = content[0]->text.front()->get_font("");
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });
}


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test3)
{
	Cleanup cl;

	auto film = new_test_film("subtitle_font_id_change_test3", {}, &cl);
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
	content[0]->only_text()->set_language(dcp::LanguageTag("de"));

	CheckContentJob check(film);
	check.run();
	BOOST_REQUIRE (!wait_for_jobs());

	auto font = content[0]->text.front()->get_font("Arial Black");
	BOOST_REQUIRE(font);
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	font = content[0]->text.front()->get_font("Helvetica Neue");
	BOOST_REQUIRE(font);
	BOOST_REQUIRE(font->file());
	BOOST_CHECK_EQUAL(*font->file(), "test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cl.run();
}


BOOST_AUTO_TEST_CASE(subtitle_font_id_change_test4)
{
	Cleanup cl;

	auto film = new_test_film("subtitle_font_id_change_test4", {}, &cl);
	boost::filesystem::remove(film->file("metadata.xml"));
	boost::filesystem::copy_file("test/data/subtitle_font_id_change_test4.xml", film->file("metadata.xml"));

	{
		Editor editor(film->file("metadata.xml"));
		editor.replace("dcpomatic-test-private", TestPaths::private_data().string());
	}

	film->read_metadata();

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_REQUIRE_EQUAL(content[0]->text.size(), 1U);

	CheckContentJob check(film);
	check.run();
	BOOST_REQUIRE(!wait_for_jobs());

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cl.run();
}

