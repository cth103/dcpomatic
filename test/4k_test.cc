/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @defgroup completedcp Complete builds of DCPs */

/** @file  test/4k_test.cc
 *  @brief Run a 4K encode from a simple input.
 *  @ingroup completedcp
 *
 *  The output is checked against test/data/4k_test.
 */


#include "lib/dcp_content_type.h"
#include "lib/dcpomatic_log.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE (fourk_test)
{
	auto film = new_test_film ("4k_test");
	LogSwitcher ls (film->log());
	film->set_name ("4k_test");
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	film->set_resolution (Resolution::FOUR_K);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	boost::filesystem::path p (test_film_dir("4k_test"));
	p /= film->dcp_name ();

	check_dcp ("test/data/4k_test", p.string());
}
