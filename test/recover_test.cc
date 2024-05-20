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
#include <dcp/equality_options.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/stereo_j2k_picture_asset.h>
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
	auto content = make_shared<FFmpegContent>("test/data/count300bd24.m2ts");
	auto film = new_test_film2("recover_test_2d", { content });
	film->set_video_bit_rate(VideoEncoding::JPEG2000, 100000000);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	auto video = [film]() {
		return find_file(boost::filesystem::path("build/test/recover_test_2d") / film->dcp_name(false), "j2c_");
	};

	boost::filesystem::copy_file (
		video(),
		"build/test/recover_test_2d/original.mxf"
		);

	boost::filesystem::resize_file(video(), 2 * 1024 * 1024);

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE
		},
		true,
		/* We end up with two CPLs in this directory, which Clairmeta gives an error for */
		false
		);

	auto A = make_shared<dcp::MonoJ2KPictureAsset>("build/test/recover_test_2d/original.mxf");
	auto B = make_shared<dcp::MonoJ2KPictureAsset>(video());

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}


BOOST_AUTO_TEST_CASE (recover_test_3d, * boost::unit_test::depends_on("recover_test_2d"))
{
	auto content = make_shared<ImageContent>("test/data/3d_test");
	content->video->set_frame_type (VideoFrameType::THREE_D_LEFT_RIGHT);
	auto film = new_test_film2("recover_test_3d", { content });
	film->set_three_d (true);
	film->set_video_bit_rate(VideoEncoding::JPEG2000, 100000000);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	auto video = [film]() {
		return find_file(boost::filesystem::path("build/test/recover_test_3d") / film->dcp_name(false), "j2c_");
	};

	boost::filesystem::copy_file (
		video(),
		"build/test/recover_test_3d/original.mxf"
		);

	boost::filesystem::resize_file(video(), 2 * 1024 * 1024);

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE
		},
		true,
		/* We end up with two CPLs in this directory, which Clairmeta gives an error for */
		false
		);

	auto A = make_shared<dcp::StereoJ2KPictureAsset>("build/test/recover_test_3d/original.mxf");
	auto B = make_shared<dcp::StereoJ2KPictureAsset>(video());

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}


BOOST_AUTO_TEST_CASE (recover_test_2d_encrypted, * boost::unit_test::depends_on("recover_test_3d"))
{
	auto content = make_shared<FFmpegContent>("test/data/count300bd24.m2ts");
	auto film = new_test_film2("recover_test_2d_encrypted", { content });
	film->set_encrypted (true);
	film->_key = dcp::Key("eafcb91c9f5472edf01f3a2404c57258");
	film->set_video_bit_rate(VideoEncoding::JPEG2000, 100000000);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE });

	auto video = [film]() {
		return find_file(boost::filesystem::path("build/test/recover_test_2d_encrypted") / film->dcp_name(false), "j2c_");
	};

	boost::filesystem::copy_file (
		video(),
		"build/test/recover_test_2d_encrypted/original.mxf"
		);

	boost::filesystem::resize_file(video(), 2 * 1024 * 1024);

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE, dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE
		},
		true,
		/* We end up with two CPLs in this directory, which Clairmeta gives an error for */
		false
		);

	auto A = make_shared<dcp::MonoJ2KPictureAsset>("build/test/recover_test_2d_encrypted/original.mxf");
	A->set_key (film->key ());
	auto B = make_shared<dcp::MonoJ2KPictureAsset>(video());
	B->set_key (film->key ());

	dcp::EqualityOptions eq;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}
