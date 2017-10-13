/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup specific
 */

#include "test.h"
#include "lib/config.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>

using std::string;
using boost::shared_ptr;

class Keep
{
public:
	Keep ()
	{
		_format = Config::instance()->dcp_asset_filename_format ();
	}

	~Keep ()
	{
		Config::instance()->set_dcp_asset_filename_format (_format);
	}

private:
	dcp::NameFormat _format;
};

BOOST_AUTO_TEST_CASE (file_naming_test)
{
	Keep k;
	Config::instance()->set_dcp_asset_filename_format (dcp::NameFormat ("%c"));

	shared_ptr<Film> film = new_test_film ("file_naming_test");
	film->set_name ("file_naming_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	shared_ptr<FFmpegContent> r (new FFmpegContent (film, "test/data/flat_red.png"));
	film->examine_and_add_content (r);
	shared_ptr<FFmpegContent> g (new FFmpegContent (film, "test/data/flat_green.png"));
	film->examine_and_add_content (g);
	shared_ptr<FFmpegContent> b (new FFmpegContent (film, "test/data/flat_blue.png"));
	film->examine_and_add_content (b);
	wait_for_jobs ();

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	film->make_dcp ();
	wait_for_jobs ();

	int got[3] = { 0, 0, 0 };
	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (film->file(film->dcp_name()));
		i != boost::filesystem::directory_iterator();
		++i) {
		if (boost::regex_match(i->path().string(), boost::regex(".*flat_red\\.png_.*\\.mxf"))) {
			++got[0];
		} else if (boost::regex_match(i->path().string(), boost::regex(".*flat_green\\.png_.*\\.mxf"))) {
			++got[1];
		} else if (boost::regex_match(i->path().string(), boost::regex(".*flat_blue\\.png_.*\\.mxf"))) {
			++got[2];
		}
	}

	for (int i = 0; i < 3; ++i) {
		BOOST_CHECK (got[i] == 2);
	}
}

BOOST_AUTO_TEST_CASE (file_naming_test2)
{
	Keep k;
	Config::instance()->set_dcp_asset_filename_format (dcp::NameFormat ("%c"));

	shared_ptr<Film> film = new_test_film ("file_naming_test2");
	film->set_name ("file_naming_test2");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	shared_ptr<FFmpegContent> r (new FFmpegContent (film, "test/data/flät_red.png"));
	film->examine_and_add_content (r);
	shared_ptr<FFmpegContent> g (new FFmpegContent (film, "test/data/flat_green.png"));
	film->examine_and_add_content (g);
	shared_ptr<FFmpegContent> b (new FFmpegContent (film, "test/data/flat_blue.png"));
	film->examine_and_add_content (b);
	wait_for_jobs ();

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	film->make_dcp ();
	wait_for_jobs ();

	int got[3] = { 0, 0, 0 };
	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (film->file(film->dcp_name()));
		i != boost::filesystem::directory_iterator();
		++i) {
		if (boost::regex_match(i->path().string(), boost::regex(".*flt_red\\.png_.*\\.mxf"))) {
			++got[0];
		} else if (boost::regex_match(i->path().string(), boost::regex(".*flat_green\\.png_.*\\.mxf"))) {
			++got[1];
		} else if (boost::regex_match(i->path().string(), boost::regex(".*flat_blue\\.png_.*\\.mxf"))) {
			++got[2];
		}
	}

	for (int i = 0; i < 3; ++i) {
		BOOST_CHECK (got[i] == 2);
	}
}
