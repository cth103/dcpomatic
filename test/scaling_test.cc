/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file test/scaling_test.cc
 *  @brief Test scaling and black-padding of images from a still-image source.
 */

#include <boost/test/unit_test.hpp>
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "test.h"

using std::string;
using boost::shared_ptr;

static void scaling_test_for (shared_ptr<Film> film, shared_ptr<VideoContent> content, string image, string container)
{
	content->set_scale (VideoContentScale (Ratio::from_id (image)));
	film->set_container (Ratio::from_id (container));
	film->make_dcp ();

	wait_for_jobs ();

	boost::filesystem::path ref;
	ref = "test";
	ref /= "data";
	ref /= "scaling_test_" + image + "_" + container;

	boost::filesystem::path check;
	check = "build";
	check /= "test";
	check /= "scaling_test";
	check /= film->dcp_name();

	check_dcp (ref.string(), check.string());
}

BOOST_AUTO_TEST_CASE (scaling_test)
{
	shared_ptr<Film> film = new_test_film ("scaling_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("scaling_test");
	shared_ptr<ImageContent> imc (new ImageContent (film, "test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (imc, true);

	wait_for_jobs ();
	
	imc->set_video_length (ContentTime::from_frames (1, 24));

	scaling_test_for (film, imc, "133", "185");
	scaling_test_for (film, imc, "185", "185");
	scaling_test_for (film, imc, "239", "185");

	scaling_test_for (film, imc, "133", "239");
	scaling_test_for (film, imc, "185", "239");
	scaling_test_for (film, imc, "239", "239");
}

