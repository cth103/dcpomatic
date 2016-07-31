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

#include "test.h"
#include "lib/config.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (file_naming_test)
{
	dcp::FilenameFormat nf ("%c");
	Config::instance()->set_dcp_filename_format (dcp::FilenameFormat ("%c"));
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

	BOOST_CHECK (boost::filesystem::exists (film->file (film->dcp_name() + "/flat_red.png.mxf")));
	BOOST_CHECK (boost::filesystem::exists (film->file (film->dcp_name() + "/flat_green.png.mxf")));
	BOOST_CHECK (boost::filesystem::exists (film->file (film->dcp_name() + "/flat_blue.png.mxf")));
}
