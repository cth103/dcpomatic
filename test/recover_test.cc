/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/recover_test.cc
 *  @brief Test recovery of a DCP transcode after a crash.
 *  @ingroup feature
 */


#include "test.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/image_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/video_content.h"
#include "lib/ratio.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static void
note (dcp::NoteType t, string n)
{
	if (t == dcp::NoteType::ERROR) {
		cout << n << "\n";
	}
}


BOOST_AUTO_TEST_CASE (recover_test_2d)
{
	auto film = new_test_film("recover_test_2d");
	film->set_interop (false);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("recover_test");

	auto content = make_shared<FFmpegContent>("test/data/count300bd24.m2ts");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	boost::filesystem::path const video = "build/test/recover_test_2d/video/185_2K_02543352c540f4b083bff3f1e309d4a9_24_100000000_P_S_L21_0_1200000.mxf";
	boost::filesystem::copy_file (
		video,
		"build/test/recover_test_2d/original.mxf"
		);

	boost::filesystem::resize_file (video, 2 * 1024 * 1024);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	auto A = make_shared<dcp::MonoPictureAsset>("build/test/recover_test_2d/original.mxf");
	auto B = make_shared<dcp::MonoPictureAsset>(video);

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}


BOOST_AUTO_TEST_CASE (recover_test_3d, * boost::unit_test::depends_on("recover_test_2d"))
{
	auto film = new_test_film ("recover_test_3d");
	film->set_interop (false);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("recover_test");
	film->set_three_d (true);

	auto content = make_shared<ImageContent>("test/data/3d_test");
	content->video->set_frame_type (VideoFrameType::THREE_D_LEFT_RIGHT);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	boost::filesystem::path const video = "build/test/recover_test_3d/video/185_2K_70e6661af92ae94458784c16a21a9748_24_100000000_P_S_L21_3D_0_96000.mxf";

	boost::filesystem::copy_file (
		video,
		"build/test/recover_test_3d/original.mxf"
		);

	boost::filesystem::resize_file (video, 2 * 1024 * 1024);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	auto A = make_shared<dcp::StereoPictureAsset>("build/test/recover_test_3d/original.mxf");
	auto B = make_shared<dcp::StereoPictureAsset>(video);

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}


BOOST_AUTO_TEST_CASE (recover_test_2d_encrypted, * boost::unit_test::depends_on("recover_test_3d"))
{
	auto film = new_test_film ("recover_test_2d_encrypted");
	film->set_interop (false);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("recover_test");
	film->set_encrypted (true);
	film->_key = dcp::Key("eafcb91c9f5472edf01f3a2404c57258");

	auto content = make_shared<FFmpegContent>("test/data/count300bd24.m2ts");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	boost::filesystem::path const video =
		"build/test/recover_test_2d_encrypted/video/185_2K_02543352c540f4b083bff3f1e309d4a9_24_100000000_Eeafcb91c9f5472edf01f3a2404c57258_S_L21_0_1200000.mxf";

	boost::filesystem::copy_file (
		video,
		"build/test/recover_test_2d_encrypted/original.mxf"
		);

	boost::filesystem::resize_file (video, 2 * 1024 * 1024);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	auto A = make_shared<dcp::MonoPictureAsset>("build/test/recover_test_2d_encrypted/original.mxf");
	A->set_key (film->key ());
	auto B = make_shared<dcp::MonoPictureAsset>(video);
	B->set_key (film->key ());

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}
