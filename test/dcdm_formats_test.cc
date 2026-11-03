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


/** @file  test/dcdm_formats_test.cc
 *  @brief Check DCDM export at every DCP frame rate, in 2D and 3D, at 4K, and that
 *  already-XYZ (e.g. HDR / PQ-coded) sources pass through with no change.
 *  @ingroup feature
 */


#include "lib/config.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcdm_film_encoder.h"
#include "lib/dpx_writer.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/job_manager.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <dcp/j2k_transcode.h>
#include <dcp/openjpeg_image.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;


/** Write @param frames DPX frames of @param size; the 12-bit value of every component of frame f
 *  is value(f), which is recoverable from the output when no colour conversion is applied.
 */
static
boost::filesystem::path
make_sequence(boost::filesystem::path dir, int frames, dcp::Size size, std::function<int (int)> value)
{
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, size, Image::Alignment::COMPACT);
	for (int f = 0; f < frames; ++f) {
		for (int y = 0; y < size.height; ++y) {
			auto row = reinterpret_cast<uint16_t*>(image->data()[0] + y * image->stride()[0]);
			for (int x = 0; x < size.width * 3; ++x) {
				row[x] = static_cast<uint16_t>(value(f) << 4);
			}
		}
		write_dpx(dir / fmt::format("src_{:05d}.dpx", f), image);
	}
	return dir;
}


static
int
count_j2k(boost::filesystem::path dir)
{
	if (!dcp::filesystem::is_directory(dir)) {
		return 0;
	}
	int n = 0;
	for (auto const& i: dcp::filesystem::directory_iterator(dir)) {
		auto const name = i.path().filename().string();
		if (name.size() > 8 && name.substr(name.size() - 8) == ".pdc.j2c") {
			++n;
		}
	}
	return n;
}


static
shared_ptr<dcp::OpenJPEGImage>
decode(boost::filesystem::path path)
{
	std::ifstream in(path.string(), std::ios::binary);
	vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return dcp::decompress_j2k(data.data(), static_cast<int64_t>(data.size()), 0);
}


static
void
run_export(shared_ptr<Film> film, boost::filesystem::path out)
{
	dcp::filesystem::remove_all(out);
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, out, DCDMPictureFormat::J2K_LOSSLESS));
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());
}


/** Export one second of film at @param rate fps, 2D or 3D, and check frame counts,
 *  WAV lengths and that each output frame is the right source frame.
 */
static
void
check_rate(int rate, bool three_d)
{
	auto const name = fmt::format("dcdm_formats_{}_{}", rate, three_d ? "3d" : "2d");
	auto film = new_test_film(name);
	film->set_video_frame_rate(rate);

	/* One second of frames; for 3D a separate left-eye and right-eye source, with different values */
	auto add_eye = [&](string eye, int offset, VideoFrameType type) {
		auto src = make_sequence("build/test/" + name + "_src_" + eye, rate, dcp::Size(64, 48), [offset](int f) { return 100 + offset + f * 7; });
		auto content = content_factory(src)[0];
		film->examine_and_add_content({content});
		BOOST_REQUIRE(!wait_for_jobs());
		content->video->unset_colour_conversion();
		content->set_video_frame_rate(film, rate);
		content->set_position(film, DCPTime());
		if (type != VideoFrameType::TWO_D) {
			content->video->set_frame_type(type);
		}
		return src;
	};

	vector<boost::filesystem::path> sources;
	if (three_d) {
		film->set_sequence(false);
		sources.push_back(add_eye("left", 0, VideoFrameType::THREE_D_LEFT));
		sources.push_back(add_eye("right", 3, VideoFrameType::THREE_D_RIGHT));
		film->set_three_d(true);
	} else {
		sources.push_back(add_eye("mono", 0, VideoFrameType::TWO_D));
	}
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_REQUIRE_EQUAL(film->video_frame_rate(), rate);
	BOOST_REQUIRE_EQUAL(film->length().frames_round(rate), rate);

	auto const out = boost::filesystem::path("build/test/" + name + "_out");
	run_export(film, out);

	vector<boost::filesystem::path> eyes;
	if (three_d) {
		eyes = { out / "picture" / "left", out / "picture" / "right" };
	} else {
		eyes = { out / "picture" };
	}
	for (size_t e = 0; e < eyes.size(); ++e) {
		BOOST_CHECK_MESSAGE(count_j2k(eyes[e]) == rate, name << ": " << count_j2k(eyes[e]) << " frames in " << eyes[e].string());
		/* Check the first, a middle and the last frame hold the source frame they should */
		for (int f: { 0, rate / 2, rate - 1 }) {
			auto const image = decode(eyes[e] / fmt::format("{:08d}.pdc.j2c", f));
			/* Right-eye values are 3 higher than left-eye ones */
			int const expected = 100 + f * 7 + (e == 1 ? 3 : 0);
			/* The small source is scaled into the middle of the container, so look at the centre pixel.
			 * Scaling may move a value by one code value (as it does for the DCP); exactness of
			 * unscaled pictures is checked by dcdm_formats_xyz_passthrough_test.
			 */
			auto const size = image->size();
			auto const centre = (size.height / 2) * size.width + size.width / 2;
			BOOST_CHECK_MESSAGE(std::abs(image->data(0)[centre] - expected) <= 1, name << " frame " << f << " eye " << e << ": " << image->data(0)[centre] << " != " << expected);
		}
	}

	/* 48kHz: rate frames make exactly one second of audio in every WAV */
	for (auto label: { "L", "R", "C", "LFE", "Ls", "Rs" }) {
		BOOST_CHECK_EQUAL(dcp::filesystem::file_size(out / "audio" / (string(label) + ".wav")), 44U + 48000U * 3U);
	}

	dcp::filesystem::remove_all(out);
	for (auto const& src: sources) {
		dcp::filesystem::remove_all(src);
	}
}


BOOST_AUTO_TEST_CASE(dcdm_formats_standard_rates_2d_test)
{
	for (auto rate: { 24, 25, 30, 48, 50, 60 }) {
		check_rate(rate, false);
	}
}


BOOST_AUTO_TEST_CASE(dcdm_formats_hfr_2d_test)
{
	/* 96, 100 and 120 fps need DCP-o-matic's "allow any DCP frame rate" setting */
	Config::instance()->set_allow_any_dcp_frame_rate(true);
	for (auto rate: { 96, 100, 120 }) {
		check_rate(rate, false);
	}
	Config::instance()->set_allow_any_dcp_frame_rate(false);
}


BOOST_AUTO_TEST_CASE(dcdm_formats_3d_test)
{
	for (auto rate: { 24, 25, 30, 48, 50, 60 }) {
		check_rate(rate, true);
	}
}


BOOST_AUTO_TEST_CASE(dcdm_formats_hfr_3d_test)
{
	Config::instance()->set_allow_any_dcp_frame_rate(true);
	for (auto rate: { 96, 100, 120 }) {
		check_rate(rate, true);
	}
	Config::instance()->set_allow_any_dcp_frame_rate(false);
}


BOOST_AUTO_TEST_CASE(dcdm_formats_4k_test)
{
	auto film = new_test_film("dcdm_formats_4k_test");
	film->set_resolution(Resolution::FOUR_K);
	auto src = make_sequence("build/test/dcdm_formats_4k_test_src", 2, dcp::Size(64, 48), [](int f) { return 1000 + f; });
	auto content = content_factory(src)[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE(!wait_for_jobs());
	content->video->unset_colour_conversion();

	auto const out = boost::filesystem::path("build/test/dcdm_formats_4k_test_out");
	run_export(film, out);

	BOOST_CHECK_EQUAL(count_j2k(out / "picture"), 24);
	auto const image = decode(out / "picture" / "00000000.pdc.j2c");
	BOOST_CHECK(image->size() == film->frame_size());
	BOOST_CHECK_EQUAL(image->size().width, 3996);
	BOOST_CHECK_EQUAL(image->size().height, 2160);

	dcp::filesystem::remove_all(out);
	dcp::filesystem::remove_all(src);
}


BOOST_AUTO_TEST_CASE(dcdm_formats_xyz_passthrough_test)
{
	/* An already-XYZ master (for example PQ-coded HDR X'Y'Z') with no colour conversion
	 * must come out with exactly the same 12-bit code values, including the extremes.
	 */
	auto film = new_test_film("dcdm_formats_xyz_passthrough_test");
	int const values[] = { 0, 1, 2048, 4094, 4095 };
	auto src = make_sequence("build/test/dcdm_formats_xyz_passthrough_test_src", 5, film->frame_size(), [&](int f) { return values[f]; });
	auto content = content_factory(src)[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE(!wait_for_jobs());
	content->video->unset_colour_conversion();
	content->set_video_frame_rate(film, 24);

	auto const out = boost::filesystem::path("build/test/dcdm_formats_xyz_passthrough_test_out");
	run_export(film, out);

	for (int f = 0; f < 5; ++f) {
		auto const image = decode(out / "picture" / fmt::format("{:08d}.pdc.j2c", f));
		auto const size = image->size();
		int64_t differences = 0;
		for (int c = 0; c < 3; ++c) {
			for (int i = 0; i < size.width * size.height; ++i) {
				if (image->data(c)[i] != values[f]) {
					++differences;
				}
			}
		}
		BOOST_CHECK_MESSAGE(differences == 0, "value " << values[f] << ": " << differences << " pixels differ");
	}

	dcp::filesystem::remove_all(out);
	dcp::filesystem::remove_all(src);
}
