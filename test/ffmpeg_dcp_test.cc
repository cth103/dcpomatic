/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "test.h"

using boost::shared_ptr;

/** @file test/ffmpeg_dcp_test.cc
 *  @brief Test scaling and black-padding of images from a still-image source.
 */

BOOST_AUTO_TEST_CASE (ffmpeg_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_dcp_test");
	film->set_name ("test_film2");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	c->set_scale (VideoContentScale (Ratio::from_id ("185")));
	film->examine_and_add_content (c);

	wait_for_jobs ();
	
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->make_dcp ();
	film->write_metadata ();

	wait_for_jobs ();
}

/** Briefly test Film::cpls().  Requires the output from ffmpeg_dcp_test above */
BOOST_AUTO_TEST_CASE (ffmpeg_have_dcp_test)
{
	boost::filesystem::path p = test_film_dir ("ffmpeg_dcp_test");
	shared_ptr<Film> f (new Film (p.string ()));
	f->read_metadata ();
	BOOST_CHECK (!f->cpls().empty());

	p /= f->dcp_name();
	p /= f->video_mxf_filename();
	boost::filesystem::remove (p);
	BOOST_CHECK (f->cpls().empty());
}
