/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::string;


BOOST_AUTO_TEST_CASE(encode_xyz_from_prores_test)
{
	auto const name = string{"encode_xyz_from_prores_test"};

	auto prores_content = content_factory(TestPaths::private_data() / "3042" / "dcp-o-matic_test_20250521.mov")[0];
	auto prores_source = new_test_film(name + "_prores");
	auto prores_film = new_test_film("encode_xyz_from_prores_test", { prores_content });
	prores_film->set_dcp_content_type(DCPContentType::from_isdcf_name("FTR"));
	prores_film->set_container(Ratio::from_id("239"));
	prores_content->video->unset_colour_conversion();
	make_and_verify_dcp(
		prores_film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	auto tiff_content = content_factory(TestPaths::private_data() / "3042" / "tiff")[0];
	auto tiff_source = new_test_film(name + "_tiff");
	auto tiff_film = new_test_film("encode_xyz_from_tiff_test", { tiff_content });
	tiff_film->set_dcp_content_type(DCPContentType::from_isdcf_name("FTR"));
	tiff_film->set_container(Ratio::from_id("239"));
	tiff_content->video->unset_colour_conversion();
	make_and_verify_dcp(
		tiff_film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	check_one_frame_against_dcp(tiff_film->dir(tiff_film->dcp_name()), 0, TestPaths::private_data() / "3042" / "dcp-o-matic_testing_from_tiff_20250521", 312, 101);
	check_one_frame_against_dcp(prores_film->dir(prores_film->dcp_name()), 312, TestPaths::private_data() / "3042" / "dcp-o-matic_testing_from_tiff_20250521", 312, 110);
}


BOOST_AUTO_TEST_CASE(encode_xyz_from_dpx_test)
{
	auto content = content_factory(TestPaths::private_data() / "count.dpx")[0];
	auto film = new_test_film("encode_xyz_from_dpx_test", { content });
	content->video->unset_colour_conversion();

	make_and_verify_dcp(film);

	check_one_frame_against_j2c(film->dir(film->dcp_name()), 0, TestPaths::private_data() / "count.j2c", 18);
}

