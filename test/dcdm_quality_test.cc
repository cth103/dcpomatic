/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  test/dcdm_quality_test.cc
 *  @brief Tests of the DCDM source quality gate.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/dcdm_quality.h"
#include "lib/dcp_content.h"
#include "lib/dpx_writer.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/image.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;


/* ProRes / DNxHR profile numbers as FFmpeg reports them */
static int constexpr PRORES_PROXY = 0;
static int constexpr PRORES_LT = 1;
static int constexpr PRORES_STANDARD = 2;
static int constexpr PRORES_HQ = 3;
static int constexpr PRORES_4444 = 4;
static int constexpr PRORES_XQ = 5;
static int constexpr DNXHR_SQ = 2;
static int constexpr DNXHR_HQ = 3;
static int constexpr DNXHR_HQX = 4;
static int constexpr DNXHR_444 = 5;


BOOST_AUTO_TEST_CASE(dcdm_ffmpeg_ok_accepts_test)
{
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_HQ, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_4444, AV_PIX_FMT_YUVA444P10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_XQ, AV_PIX_FMT_YUVA444P12LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_DNXHD, DNXHR_HQX, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_DNXHD, DNXHR_444, AV_PIX_FMT_GBRP10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_FFV1, 0, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_V210, 0, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(dcdm_ffmpeg_ok(AV_CODEC_ID_RAWVIDEO, 0, AV_PIX_FMT_YUV422P10LE));
}


BOOST_AUTO_TEST_CASE(dcdm_ffmpeg_ok_rejects_test)
{
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_PROXY, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_LT, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_STANDARD, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_DNXHD, DNXHR_SQ, AV_PIX_FMT_YUV422P));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_DNXHD, DNXHR_HQ, AV_PIX_FMT_YUV422P));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_H264, 100, AV_PIX_FMT_YUV420P));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_H264, 122, AV_PIX_FMT_YUV422P10LE));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_HEVC, 2, AV_PIX_FMT_YUV420P10LE));
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_MPEG2VIDEO, 0, AV_PIX_FMT_YUV422P));
	/* Good codec and profile but 8 bits */
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_PRORES, PRORES_HQ, AV_PIX_FMT_YUV422P));
	/* Good codec and bit depth but 4:2:0 */
	BOOST_CHECK(!dcdm_ffmpeg_ok(AV_CODEC_ID_FFV1, 0, AV_PIX_FMT_YUV420P10LE));
}


BOOST_AUTO_TEST_CASE(dcdm_image_ok_test)
{
	BOOST_CHECK(dcdm_image_ok("a/b/frame_0001.tiff", AV_PIX_FMT_RGB48LE));
	BOOST_CHECK(dcdm_image_ok("a/b/frame_0001.TIF", AV_PIX_FMT_RGB48LE));
	BOOST_CHECK(dcdm_image_ok("a/b/frame_0001.dpx", AV_PIX_FMT_RGB48LE));
	BOOST_CHECK(dcdm_image_ok("a/b/frame_0001.dpx", AV_PIX_FMT_GBRP12LE));
	BOOST_CHECK(dcdm_image_ok("a/b/frame_0001.exr", AV_PIX_FMT_GBRPF32LE));
	BOOST_CHECK(!dcdm_image_ok("a/b/frame_0001.jpg", AV_PIX_FMT_YUVJ420P));
	BOOST_CHECK(!dcdm_image_ok("a/b/frame_0001.png", AV_PIX_FMT_RGB24));
	BOOST_CHECK(!dcdm_image_ok("a/b/frame_0001.png", AV_PIX_FMT_RGB48BE));
	BOOST_CHECK(!dcdm_image_ok("a/b/frame_0001.tiff", AV_PIX_FMT_RGB24));
	BOOST_CHECK(!dcdm_image_ok("a/b/frame_0001.j2c", AV_PIX_FMT_XYZ12LE));
}


/** Make a single-frame DPX that qualifies as a DCDM source */
static
boost::filesystem::path
make_good_dpx(string name)
{
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(64, 48), Image::Alignment::COMPACT);
	memset(image->data()[0], 0x40, 48 * image->stride()[0]);
	boost::filesystem::path path = "build/test/" + name + ".dpx";
	write_dpx(path, image);
	return path;
}


BOOST_AUTO_TEST_CASE(dcdm_empty_film_test)
{
	auto film = new_test_film("dcdm_empty_film_test");
	auto const result = dcdm_source_quality(film);
	BOOST_CHECK(!result.ok);
	BOOST_CHECK(!result.reason.empty());
}


BOOST_AUTO_TEST_CASE(dcdm_good_film_test)
{
	auto film = new_test_film("dcdm_good_film_test", content_factory(make_good_dpx("dcdm_good_film_test")));
	auto const result = dcdm_source_quality(film);
	BOOST_CHECK_MESSAGE(result.ok, result.reason);
	BOOST_CHECK(result.reason.empty());
}


BOOST_AUTO_TEST_CASE(dcdm_mixed_film_test)
{
	auto good = content_factory(make_good_dpx("dcdm_mixed_film_test"));
	auto bad = content_factory("test/data/10bit_video_422.mp4");
	auto film = new_test_film("dcdm_mixed_film_test", { good[0], bad[0] });
	auto const result = dcdm_source_quality(film);
	BOOST_CHECK(!result.ok);
	BOOST_CHECK_MESSAGE(result.reason.find("10bit_video_422.mp4") != string::npos, result.reason);
}


BOOST_AUTO_TEST_CASE(dcdm_dcp_content_fails_test)
{
	auto ov = new_test_film("dcdm_dcp_content_fails_test_ov", content_factory("test/data/flat_red.png"));
	make_and_verify_dcp(ov);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name(false)));
	auto film = new_test_film("dcdm_dcp_content_fails_test", { dcp });
	auto const result = dcdm_source_quality(film);
	BOOST_CHECK(!result.ok);
	BOOST_CHECK_MESSAGE(result.reason.find("DCP") != string::npos, result.reason);
}


BOOST_AUTO_TEST_CASE(dcdm_legacy_metadata_fails_test)
{
	auto film = new_test_film("dcdm_legacy_metadata_fails_test", content_factory("test/data/10bit_video_422.mp4"));
	film->write_metadata();

	auto const metadata = *film->directory() / "metadata.xml";
	string text;
	{
		std::ifstream in(metadata.string());
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	for (auto tag: { "VideoCodecId", "VideoProfile", "VideoPixelFormat" }) {
		string const open = string("<") + tag + ">";
		string const close = string("</") + tag + ">";
		auto start = text.find(open);
		BOOST_REQUIRE(start != string::npos);
		text.erase(start, text.find(close, start) + close.size() - start);
	}
	{
		std::ofstream out(metadata.string());
		out << text;
	}

	auto reread = make_shared<Film>(film->directory().get());
	reread->read_metadata();
	auto const result = dcdm_source_quality(reread);
	BOOST_CHECK(!result.ok);
	BOOST_CHECK_MESSAGE(result.reason.find("examine") != string::npos, result.reason);
}
