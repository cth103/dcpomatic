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


/** @file  test/source_metadata_test.cc
 *  @brief Tests of the source codec and pixel format kept by FFmpegContent and ImageContent.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "test.h"
#include <dcp/filesystem.h>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>


using std::dynamic_pointer_cast;
using std::make_shared;


BOOST_AUTO_TEST_CASE(ffmpeg_content_codec_metadata_test)
{
	auto content = content_factory("test/data/10bit_video_422.mp4")[0];
	auto film = new_test_film("ffmpeg_content_codec_metadata_test", { content });

	auto ffmpeg = dynamic_pointer_cast<FFmpegContent>(content);
	BOOST_REQUIRE(ffmpeg);
	BOOST_REQUIRE(ffmpeg->video_codec_id());
	BOOST_CHECK_EQUAL(*ffmpeg->video_codec_id(), AV_CODEC_ID_H264);
	BOOST_REQUIRE(ffmpeg->video_pixel_format());
	BOOST_CHECK_EQUAL(*ffmpeg->video_pixel_format(), AV_PIX_FMT_YUV422P10LE);
	BOOST_REQUIRE(ffmpeg->video_profile());

	film->write_metadata();
	auto reread = make_shared<Film>(film->directory().get());
	reread->read_metadata();
	auto ffmpeg2 = dynamic_pointer_cast<FFmpegContent>(reread->content().front());
	BOOST_REQUIRE(ffmpeg2);
	BOOST_CHECK(ffmpeg2->video_codec_id() == ffmpeg->video_codec_id());
	BOOST_CHECK(ffmpeg2->video_profile() == ffmpeg->video_profile());
	BOOST_CHECK(ffmpeg2->video_pixel_format() == ffmpeg->video_pixel_format());
}


BOOST_AUTO_TEST_CASE(image_content_pixel_format_test)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("image_content_pixel_format_test", { content });

	auto image = dynamic_pointer_cast<ImageContent>(content);
	BOOST_REQUIRE(image);
	BOOST_REQUIRE(image->pixel_format());
	BOOST_CHECK_EQUAL(*image->pixel_format(), AV_PIX_FMT_RGB24);

	film->write_metadata();
	auto reread = make_shared<Film>(film->directory().get());
	reread->read_metadata();
	auto image2 = dynamic_pointer_cast<ImageContent>(reread->content().front());
	BOOST_REQUIRE(image2);
	BOOST_CHECK(image2->pixel_format() == image->pixel_format());
}


BOOST_AUTO_TEST_CASE(old_metadata_test)
{
	auto content = content_factory("test/data/10bit_video_422.mp4")[0];
	auto image_content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("old_metadata_test", { content, image_content });
	film->write_metadata();

	/* Remove the new tags to simulate metadata written before they existed */
	auto const metadata = *film->directory() / "metadata.xml";
	std::string text;
	{
		std::ifstream in(metadata.string());
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	for (auto tag: { "VideoCodecId", "VideoProfile", "VideoPixelFormat", "PixelFormat" }) {
		std::string const open = std::string("<") + tag + ">";
		std::string const close = std::string("</") + tag + ">";
		for (auto start = text.find(open); start != std::string::npos; start = text.find(open)) {
			auto end = text.find(close, start);
			BOOST_REQUIRE(end != std::string::npos);
			text.erase(start, end + close.size() - start);
		}
	}
	{
		std::ofstream out(metadata.string());
		out << text;
	}

	auto reread = make_shared<Film>(film->directory().get());
	reread->read_metadata();
	BOOST_REQUIRE_EQUAL(reread->content().size(), 2U);
	for (auto c: reread->content()) {
		if (auto ffmpeg = dynamic_pointer_cast<FFmpegContent>(c)) {
			BOOST_CHECK(!ffmpeg->video_codec_id());
			BOOST_CHECK(!ffmpeg->video_profile());
			BOOST_CHECK(!ffmpeg->video_pixel_format());
		} else if (auto image = dynamic_pointer_cast<ImageContent>(c)) {
			BOOST_CHECK(!image->pixel_format());
		}
	}
}
