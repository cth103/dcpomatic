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


/** @file  test/j2k_video_bit_rate_test.cc
 *  @brief Test whether we output whatever J2K bandwidth is requested.
 *  @ingroup feature
 */


#include "test.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/video_content.h"
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;


static void
check (int target_bits_per_second)
{
	int const duration = 10;

	string const name = "bandwidth_test_" + dcp::raw_convert<string> (target_bits_per_second);
	auto film = new_test_film (name);
	film->set_name (name);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_video_bit_rate(VideoEncoding::JPEG2000, target_bits_per_second);
	auto content = make_shared<ImageContent>(TestPaths::private_data() / "prophet_frame.tiff");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	content->video->set_length (24 * duration);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::NEARLY_INVALID_PICTURE_FRAME_SIZE_IN_BYTES,
			dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_SIZE_IN_BYTES,
			dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PART_SIZE,
		},
		target_bits_per_second <= 250000000,
		target_bits_per_second <= 250000000
		);

	boost::filesystem::directory_iterator i (boost::filesystem::path("build") / "test" / name / "video");
	boost::filesystem::path test = *i++;
	BOOST_REQUIRE (i == boost::filesystem::directory_iterator());

	double actual_bits_per_second = boost::filesystem::file_size(test) * 8.0 / duration;

	/* Check that we're within 85% to 115% of target on average */
	BOOST_CHECK ((actual_bits_per_second / target_bits_per_second) > 0.85);
	BOOST_CHECK ((actual_bits_per_second / target_bits_per_second) < 1.15);
}


BOOST_AUTO_TEST_CASE (bandwidth_test)
{
	check (50000000);
	check (100000000);
	check (150000000);
	check (200000000);
	check (250000000);
	check (300000000);
	check (350000000);
	check (400000000);
	check (450000000);
	check (500000000);
}
