/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file test/scaling_test.cc
 *  @brief Test scaling and black-padding of images from a still-image source.
 *  @ingroup feature
 */

#include <boost/test/unit_test.hpp>
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "test.h"

using std::string;
using boost::shared_ptr;

static void scaling_test_for (shared_ptr<Film> film, shared_ptr<Content> content, float ratio, std::string image, string container)
{
	content->video->set_custom_ratio (ratio);
	film->set_container (Ratio::from_id (container));
	film->set_interop (false);
	film->make_dcp ();

	BOOST_REQUIRE (!wait_for_jobs());

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
	shared_ptr<ImageContent> imc (new ImageContent("test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (imc);

	BOOST_REQUIRE (!wait_for_jobs());

	imc->video->set_length (1);

	/* F-133: 133 image in a flat container */
	scaling_test_for (film, imc, 4.0 / 3, "133", "185");
	/* F: flat image in a flat container */
	scaling_test_for (film, imc, 1.85, "185", "185");
	/* F-S: scope image in a flat container */
	scaling_test_for (film, imc, 2.38695, "239", "185");

	/* S-133: 133 image in a scope container */
	scaling_test_for (film, imc, 4.0 / 3, "133", "239");
	/* S-F: flat image in a scope container */
	scaling_test_for (film, imc, 1.85, "185", "239");
	/* S: scope image in a scope container */
	scaling_test_for (film, imc, 2.38695, "239", "239");
}
