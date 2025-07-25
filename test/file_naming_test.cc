/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/file_naming_test.cc
 *  @brief Test how files in DCPs are named.
 *  @ingroup feature
 */


#include "test.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/video_content.h"
#ifdef DCPOMATIC_WINDOWS
#include <boost/locale.hpp>
#endif
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>


using std::make_shared;
using std::string;


static
string
mxf_regex(string part) {
#ifdef DCPOMATIC_WINDOWS
	/* Windows replaces . in filenames with _ */
	return fmt::format(".*flat_{}_png_.*\\.mxf", part);
#else
	return fmt::format(".*flat_{}\\.png_.*\\.mxf", part);
#endif
};



BOOST_AUTO_TEST_CASE (file_naming_test)
{
	ConfigRestorer cr;
	Config::instance()->set_dcp_asset_filename_format (dcp::NameFormat("%c"));

	auto r = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto g = make_shared<FFmpegContent>("test/data/flat_green.png");
	auto b = make_shared<FFmpegContent>("test/data/flat_blue.png");
	auto film = new_test_film("file_naming_test", { r, g, b });
	film->set_video_frame_rate (24);

	r->set_position (film, dcpomatic::DCPTime::from_seconds(0));
	r->set_video_frame_rate(film, 24);
	r->video->set_length (24);
	g->set_position (film, dcpomatic::DCPTime::from_seconds(1));
	g->set_video_frame_rate(film, 24);
	g->video->set_length (24);
	b->set_position (film, dcpomatic::DCPTime::from_seconds(2));
	b->set_video_frame_rate(film, 24);
	b->video->set_length (24);

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	film->write_metadata ();
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	int got[3] = { 0, 0, 0 };
	for (auto i: boost::filesystem::directory_iterator(film->file(film->dcp_name()))) {
		if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("red")))) {
			++got[0];
		} else if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("green")))) {
			++got[1];
		} else if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("blue")))) {
			++got[2];
		}
	}

	for (int i = 0; i < 3; ++i) {
		BOOST_CHECK (got[i] == 2);
	}
}


BOOST_AUTO_TEST_CASE (file_naming_test2)
{
	ConfigRestorer cr;

	Config::instance()->set_dcp_asset_filename_format (dcp::NameFormat ("%c"));

	auto r = make_shared<FFmpegContent>("test/data/flät_red.png");
	auto g = make_shared<FFmpegContent>("test/data/flat_green.png");
	auto b = make_shared<FFmpegContent>("test/data/flat_blue.png");
	auto film = new_test_film("file_naming_test2", { r, g, b });

	r->set_position (film, dcpomatic::DCPTime::from_seconds(0));
	r->set_video_frame_rate(film, 24);
	r->video->set_length (24);
	g->set_position (film, dcpomatic::DCPTime::from_seconds(1));
	g->set_video_frame_rate(film, 24);
	g->video->set_length (24);
	b->set_position (film, dcpomatic::DCPTime::from_seconds(2));
	b->set_video_frame_rate(film, 24);
	b->video->set_length (24);

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	int got[3] = { 0, 0, 0 };
	for (auto i: boost::filesystem::directory_iterator (film->file(film->dcp_name()))) {
		if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("red")))) {
			++got[0];
		} else if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("green")))) {
			++got[1];
		} else if (boost::regex_match(i.path().string(), boost::regex(mxf_regex("blue")))) {
			++got[2];
		}
	}

	for (int i = 0; i < 3; ++i) {
		BOOST_CHECK (got[i] == 2);
	}
}


BOOST_AUTO_TEST_CASE (subtitle_file_naming)
{
	ConfigRestorer cr;

	Config::instance()->set_dcp_asset_filename_format(dcp::NameFormat("%t ostrabagalous %c"));

	auto content = content_factory("test/data/15s.srt");
	auto film = new_test_film("subtitle_file_naming", content);
	film->set_interop(false);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		});

	int got = 0;

	for (auto i: boost::filesystem::directory_iterator(film->file(film->dcp_name()))) {
		if (boost::regex_match(i.path().filename().string(), boost::regex("sub_ostrabagalous_15s.*\\.mxf"))) {
			++got;
		}
	}

	BOOST_CHECK_EQUAL(got, 1);
}


BOOST_AUTO_TEST_CASE(remove_bad_characters_from_template)
{
	ConfigRestorer cr;

	/* %z is not recognised, so the % should be discarded so it won't trip
	 * an invalid URI check in make_and_verify_dcp
	 */
	Config::instance()->set_dcp_asset_filename_format(dcp::NameFormat("%c%z"));

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film("remove_bad_characters_from_template", content);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});
}

