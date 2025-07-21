/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/video_level_test.cc
 *  @brief Test that video level ranges are handled correctly.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/content_video.h"
#include "lib/dcp_content.h"
#include "lib/decoder_factory.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/ffmpeg_image_proxy.h"
#include "lib/image.h"
#include "lib/image_content.h"
#include "lib/image_decoder.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/job_manager.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/transcode_job.h"
#include "lib/video_decoder.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/mono_j2k_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::min;
using std::max;
using std::pair;
using std::string;
using std::dynamic_pointer_cast;
using std::make_shared;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using std::shared_ptr;


static
shared_ptr<Image>
grey_image (dcp::Size size, uint8_t pixel)
{
	auto grey = make_shared<Image>(AV_PIX_FMT_RGB24, size, Image::Alignment::PADDED);
	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = grey->data()[0] + y * grey->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			*p++ = pixel;
			*p++ = pixel;
			*p++ = pixel;
		}
	}

	return grey;
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_full_range_not_changed)
{
	dcp::Size size(640, 480);
	uint8_t const grey_pixel = 128;
	boost::filesystem::path const file = "build/test/ffmpeg_image_full_range_not_changed.png";

	write_image (grey_image(size, grey_pixel), file);

	FFmpegImageProxy proxy (file);
	ImageProxy::Result result = proxy.image (Image::Alignment::COMPACT);
	BOOST_REQUIRE (!result.error);

	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = result.image->data()[0] + y * result.image->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			BOOST_REQUIRE (*p++ == grey_pixel);
		}
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_video_range_expanded)
{
	dcp::Size size(1998, 1080);
	uint8_t const grey_pixel = 128;
	uint8_t const expanded_grey_pixel = static_cast<uint8_t>(lrintf((grey_pixel - 16) * 256.0 / 219));
	boost::filesystem::path const file = "build/test/ffmpeg_image_video_range_expanded.png";

	write_image(grey_image(size, grey_pixel), file);

	auto content = content_factory(file);
	auto film = new_test_film("ffmpeg_image_video_range_expanded", content);
	content[0]->video->set_range (VideoRange::VIDEO);
	auto player = make_shared<Player>(film, film->playlist(), false);

	shared_ptr<PlayerVideo> player_video;
	player->Video.connect([&player_video](shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime) {
		player_video = pv;
	});
	while (!player_video) {
		BOOST_REQUIRE (!player->pass());
	}

	auto image = player_video->image(force(AV_PIX_FMT_RGB24), VideoRange::FULL, false);

	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = image->data()[0] + y * image->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			BOOST_REQUIRE_EQUAL (*p++, expanded_grey_pixel);
		}
	}
}


BOOST_AUTO_TEST_CASE(yuv_expanded_into_full_rgb)
{
	auto convert = [](int y_val, int u_val, int v_val, AVPixelFormat pix_fmt) {
		auto const size = dcp::Size(640, 480);
		auto yuv = make_shared<Image>(AV_PIX_FMT_YUVA444P12LE, size, Image::Alignment::PADDED);
		BOOST_REQUIRE_EQUAL(yuv->planes(), 4);
		for (int y = 0; y < size.height; ++y) {
			uint16_t* Y = reinterpret_cast<uint16_t*>(yuv->data()[0] + y * yuv->stride()[0]);
			uint16_t* U = reinterpret_cast<uint16_t*>(yuv->data()[1] + y * yuv->stride()[1]);
			uint16_t* V = reinterpret_cast<uint16_t*>(yuv->data()[2] + y * yuv->stride()[2]);
			uint16_t* A = reinterpret_cast<uint16_t*>(yuv->data()[3] + y * yuv->stride()[3]);
			for (int x = 0; x < size.width; ++x) {
				*Y++ = y_val;
				*U++ = u_val;
				*V++ = v_val;
				*A++ = 4096;
			}
		}

		return yuv->crop_scale_window(
			Crop(), size, size, dcp::YUVToRGB::REC709,
			VideoRange::VIDEO,
			pix_fmt,
			VideoRange::FULL,
			Image::Alignment::COMPACT,
			false
			);
	};

	auto white24 = convert(3760, 2048, 2048, AV_PIX_FMT_RGB24);
	BOOST_CHECK_EQUAL(white24->data()[0][0], 255);
	BOOST_CHECK_EQUAL(white24->data()[0][1], 255);
	BOOST_CHECK_EQUAL(white24->data()[0][2], 255);

	auto black24 = convert(256, 2048, 2048, AV_PIX_FMT_RGB24);
	BOOST_CHECK_EQUAL(black24->data()[0][0], 0);
	BOOST_CHECK_EQUAL(black24->data()[0][1], 0);
	BOOST_CHECK_EQUAL(black24->data()[0][2], 0);

	auto white48 = convert(3760, 2048, 2048, AV_PIX_FMT_RGB48LE);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(white48->data()[0])[0], 65283);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(white48->data()[0])[1], 65283);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(white48->data()[0])[2], 65283);

	auto black48 = convert(256, 2048, 2048, AV_PIX_FMT_RGB48LE);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(black48->data()[0])[0], 0);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(black48->data()[0])[1], 0);
	BOOST_CHECK_EQUAL(reinterpret_cast<uint16_t*>(black48->data()[0])[2], 0);
}


static
pair<int, int>
pixel_range (shared_ptr<const Image> image)
{
	pair<int, int> range(INT_MAX, 0);
	switch (image->pixel_format()) {
	case AV_PIX_FMT_RGB24:
	{
		dcp::Size const size = image->sample_size(0);
		for (int y = 0; y < size.height; ++y) {
			uint8_t* p = image->data()[0] + y * image->stride()[0];
			for (int x = 0; x < size.width * 3; ++x) {
				range.first = min(range.first, static_cast<int>(*p));
				range.second = max(range.second, static_cast<int>(*p));
				++p;
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV444P:
	{
		for (int c = 0; c < 3; ++c) {
			dcp::Size const size = image->sample_size(c);
			for (int y = 0; y < size.height; ++y) {
				uint8_t* p = image->data()[c] + y * image->stride()[c];
				for (int x = 0; x < size.width; ++x) {
					range.first = min(range.first, static_cast<int>(*p));
					range.second = max(range.second, static_cast<int>(*p));
					++p;
				}
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV422P10LE:
	case AV_PIX_FMT_YUV444P10LE:
	case AV_PIX_FMT_YUV444P12LE:
	{
		for (int c = 0; c < 3; ++c) {
			dcp::Size const size = image->sample_size(c);
			for (int y = 0; y < size.height; ++y) {
				uint16_t* p = reinterpret_cast<uint16_t*>(image->data()[c]) + y * image->stride()[c] / 2;
				for (int x = 0; x < size.width; ++x) {
					range.first = min(range.first, static_cast<int>(*p));
					range.second = max(range.second, static_cast<int>(*p));
					++p;
				}
			}
		}
		break;
	}
	default:
		BOOST_REQUIRE_MESSAGE (false, "No support for pixel format " << image->pixel_format());
	}

	return range;
}


/** @return pixel range of the first frame in @ref content in its raw form, i.e.
 *  straight out of the decoder with no level processing, scaling etc.
 */
static
pair<int, int>
pixel_range (shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	auto decoder = decoder_factory(film, content, false, false, shared_ptr<Decoder>());
	optional<ContentVideo> content_video;
	decoder->video->Data.connect ([&content_video](ContentVideo cv) {
		content_video = cv;
	});
	while (!content_video) {
		BOOST_REQUIRE (!decoder->pass());
	}

	return pixel_range (content_video->image->image(Image::Alignment::COMPACT).image);
}


static
pair<int, int>
pixel_range (boost::filesystem::path dcp_path)
{
	dcp::DCP dcp (dcp_path);
	dcp.read ();

	auto picture = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(dcp.cpls().front()->reels().front()->main_picture()->asset());
	BOOST_REQUIRE (picture);
	auto frame = picture->start_read()->get_frame(0)->xyz_image();

	int const width = frame->size().width;
	int const height = frame->size().height;

	pair<int, int> range(INT_MAX, 0);
	for (int c = 0; c < 3; ++c) {
		for (int y = 0; y < height; ++y) {
			int* p = frame->data(c) + y * width;
			for (int x = 0; x < width; ++x) {
				range.first = min(range.first, *p);
				range.second = max(range.second, *p);
				++p;
			}
		}
	}

	return range;
}


/* Functions to make a Film with different sorts of content.
 *
 * In these names V = video range (limited)
 *                F = full range  (not limited)
 *                o = overridden
 */


static
shared_ptr<Film>
movie_V (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<FFmpegContent>(content_factory("test/data/rgb_grey_testcard.mp4")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	auto range = pixel_range (film, content);
	BOOST_CHECK_EQUAL (range.first, 15);
	BOOST_CHECK_EQUAL (range.second, 243);

	return film;
}


/** @return Film containing video-range content set as full-range */
static
shared_ptr<Film>
movie_VoF (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<FFmpegContent>(content_factory("test/data/rgb_grey_testcard.mp4")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());
	content->video->set_range (VideoRange::FULL);

	auto range = pixel_range (film, content);
	BOOST_CHECK_EQUAL (range.first, 15);
	BOOST_CHECK_EQUAL (range.second, 243);

	return film;
}


static
shared_ptr<Film>
movie_F (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<FFmpegContent>(content_factory("test/data/rgb_grey_testcard.mov")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_CHECK(content->video->range() == VideoRange::FULL);

	auto range = pixel_range (film, content);
	BOOST_CHECK_EQUAL (range.first, 0);
	BOOST_CHECK_EQUAL (range.second, 1023);

	return film;
}


static
shared_ptr<Film>
movie_FoV (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<FFmpegContent>(content_factory("test/data/rgb_grey_testcard.mov")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());
	content->video->set_range (VideoRange::VIDEO);

	auto range = pixel_range (film, content);
	BOOST_CHECK_EQUAL (range.first, 0);
	BOOST_CHECK_EQUAL (range.second, 1023);

	return film;
}


static
shared_ptr<Film>
image_F (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<ImageContent>(content_factory("test/data/rgb_grey_testcard.png")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	auto range = pixel_range (film, content);
	BOOST_CHECK_EQUAL (range.first, 0);
	BOOST_CHECK_EQUAL (range.second, 255);

	return film;
}


static
shared_ptr<Film>
image_FoV (string name)
{
	auto film = new_test_film(name);
	auto content = dynamic_pointer_cast<ImageContent>(content_factory("test/data/rgb_grey_testcard.png")[0]);
	BOOST_REQUIRE (content);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());
	content->video->set_range (VideoRange::VIDEO);

	auto range = pixel_range (film, content);
	/* We are taking some full-range content and saying it should be read as video range, after which its
	 * pixels will still be full range.
	 */
	BOOST_CHECK_EQUAL (range.first, 0);
	BOOST_CHECK_EQUAL (range.second, 255);

	return film;
}


static
shared_ptr<Film>
dcp_F (string name)
{
	boost::filesystem::path const dcp = "test/data/RgbGreyTestcar_TST-1_F_MOS_2K_20201115_SMPTE_OV";
	auto film = new_test_film(name);
	auto content = make_shared<DCPContent>(dcp);
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	auto range = pixel_range (dcp);
	BOOST_CHECK_EQUAL (range.first, 0);
	BOOST_CHECK_EQUAL (range.second, 4081);

	return film;
}



/* Functions to get the pixel range in different sorts of output */


/** Get the pixel range in a DCP made from film */
static
pair<int, int>
dcp_range (shared_ptr<Film> film)
{
	make_and_verify_dcp (film);
	return pixel_range (film->dir(film->dcp_name()));
}


/** Get the pixel range in a video-range movie exported from film */
static
pair<int, int>
V_movie_range (shared_ptr<Film> film)
{
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder (
		make_shared<FFmpegFilmEncoder>(film, job, film->file("export.mov"), ExportFormat::PRORES_HQ, true, false, false, 23)
		);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());

	/* This is a bit of a hack; add the exported file into the project so we can decode it */
	auto content = make_shared<FFmpegContent>(film->file("export.mov"));
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	return pixel_range (film, content);
}


/* The tests */


BOOST_AUTO_TEST_CASE (movie_V_to_dcp)
{
	auto range = dcp_range (movie_V("movie_V_to_dcp"));
	/* Video range has been correctly expanded to full for the DCP */
	check_int_close(range, {0, 4081}, 2);
}


BOOST_AUTO_TEST_CASE (movie_VoF_to_dcp)
{
	auto range = dcp_range (movie_VoF("movie_VoF_to_dcp"));
	/* We said that video range data was really full range, so here we are in the DCP
	 * with video-range data.
	 */
	check_int_close (range, {350, 3832}, 2);
}


BOOST_AUTO_TEST_CASE (movie_F_to_dcp)
{
	auto range = dcp_range (movie_F("movie_F_to_dcp"));
	/* The nearly-full-range of the input has been preserved */
	check_int_close(range, {0, 4080}, 2);
}


BOOST_AUTO_TEST_CASE (video_FoV_to_dcp)
{
	auto range = dcp_range (movie_FoV("video_FoV_to_dcp"));
	/* The nearly-full-range of the input has become even more full, and clipped */
	check_int_close(range, {0, 4093}, 2);
}


BOOST_AUTO_TEST_CASE (image_F_to_dcp)
{
	auto range = dcp_range (image_F("image_F_to_dcp"));
	check_int_close(range, {0, 4080}, 3);
}


BOOST_AUTO_TEST_CASE (image_FoV_to_dcp)
{
	auto range = dcp_range (image_FoV("image_FoV_to_dcp"));
	/* The nearly-full-range of the input has become even more full, and clipped.
	 * XXX: I'm not sure why this doesn't quite hit 4095.
	 */
	check_int_close (range, {0, 4095}, 16);
}


BOOST_AUTO_TEST_CASE (movie_V_to_V_movie)
{
	auto range = V_movie_range (movie_V("movie_V_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 60);
	BOOST_CHECK_EQUAL (range.second, 998);
}


BOOST_AUTO_TEST_CASE (movie_VoF_to_V_movie)
{
	auto range = V_movie_range (movie_VoF("movie_VoF_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 116);
	BOOST_CHECK_EQUAL (range.second, 939);
}


BOOST_AUTO_TEST_CASE (movie_F_to_V_movie)
{
	auto range = V_movie_range (movie_F("movie_F_to_V_movie"));
	/* A full range input has been converted to video range, so that what was black at 0
	 * is not black at 64 (with the corresponding change to white)
	 */
	BOOST_CHECK_EQUAL(range.first, 64);
	BOOST_CHECK_EQUAL(range.second, 963);
}


BOOST_AUTO_TEST_CASE (movie_FoV_to_V_movie)
{
	auto range = V_movie_range (movie_FoV("movie_FoV_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 4);
	BOOST_CHECK_EQUAL (range.second, 1019);
}


BOOST_AUTO_TEST_CASE (image_F_to_V_movie)
{
	auto range = V_movie_range (image_F("image_F_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 64);
	BOOST_CHECK_EQUAL (range.second, 960);
}


BOOST_AUTO_TEST_CASE (image_FoV_to_V_movie)
{
	auto range = V_movie_range (image_FoV("image_FoV_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 64);
	BOOST_CHECK_EQUAL (range.second, 960);
}


BOOST_AUTO_TEST_CASE (dcp_F_to_V_movie)
{
	auto range = V_movie_range (dcp_F("dcp_F_to_V_movie"));
	BOOST_CHECK_EQUAL (range.first, 64);
	BOOST_CHECK_EQUAL (range.second, 944);
}

