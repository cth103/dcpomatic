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


/** @file  test/dcdm_film_encoder_test.cc
 *  @brief Tests of DCDMFilmEncoder.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/audio_processor.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcdm_film_encoder.h"
#include "lib/dcdm_quality.h"
#include "lib/wav_writer.h"
#include "lib/cross.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/atmos_metadata.h"
#include "lib/dpx_writer.h"
#include "lib/film.h"
#include "lib/dcp_text_track.h"
#include "lib/image.h"
#include "lib/job_manager.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_reader.h>
#include <dcp/atmos_frame.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/filesystem.h>
#include <dcp/interop_text_asset.h>
#include <dcp/j2k_transcode.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel_text_asset.h>
#include <dcp/smpte_text_asset.h>
#include <dcp/text_image.h>
#include <dcp/text_string.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


/** Write @param frames qualifying 12-bit DPX frames into @param dir and return the directory */
static
boost::filesystem::path
make_dpx_sequence(boost::filesystem::path dir, int frames)
{
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(64, 48), Image::Alignment::COMPACT);
	for (int i = 0; i < frames; ++i) {
		memset(image->data()[0], 0x10 * (i + 1), 48 * image->stride()[0]);
		char name[64];
		snprintf(name, sizeof(name), "src_%04d.dpx", i);
		write_dpx(dir / name, image);
	}
	return dir;
}


static
shared_ptr<Content>
dpx_content(shared_ptr<Film> film, boost::filesystem::path dir)
{
	auto content = content_factory(dir)[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE(!wait_for_jobs());
	content->set_video_frame_rate(film, 24);
	return content;
}


static
int
count_files(boost::filesystem::path dir, string extension)
{
	if (!dcp::filesystem::is_directory(dir)) {
		return 0;
	}
	int n = 0;
	for (auto const& i: dcp::filesystem::directory_iterator(dir)) {
		if (i.path().extension() == extension) {
			++n;
		}
	}
	return n;
}


static
void
run_export(shared_ptr<Film> film, boost::filesystem::path output, DCDMPictureFormat format = DCDMPictureFormat::DPX)
{
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, output, format));
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());
}


static
void
check_audio(boost::filesystem::path dir, vector<string> labels, int video_frames)
{
	for (auto const& label: labels) {
		auto const path = dir / "audio" / (label + ".wav");
		BOOST_REQUIRE_MESSAGE(dcp::filesystem::exists(path), path.string());
		/* 24-bit mono at 48kHz, 2000 samples per frame at 24fps */
		BOOST_CHECK_EQUAL(dcp::filesystem::file_size(path), 44U + video_frames * 2000U * 3U);
	}
	BOOST_CHECK_EQUAL(count_files(dir / "audio", ".wav"), static_cast<int>(labels.size()));
}


vector<string> const five_one = { "L", "R", "C", "LFE", "Ls", "Rs" };


BOOST_AUTO_TEST_CASE(dcdm_encoder_2d_test)
{
	auto film = new_test_film("dcdm_encoder_2d_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_2d_test_src", 6));
	auto const out = boost::filesystem::path("build/test/dcdm_encoder_2d_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	/* Films are at least one second long, so the 6 source frames are padded to 24 */
	auto const frames = film->length().frames_round(24);
	BOOST_REQUIRE_EQUAL(frames, 24);
	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), frames);
	BOOST_CHECK(dcp::filesystem::exists(out / "frame_00000000.dpx"));
	BOOST_CHECK(dcp::filesystem::exists(out / "frame_00000023.dpx"));
	auto const size = film->frame_size();
	BOOST_CHECK_EQUAL(dcp::filesystem::file_size(out / "frame_00000000.dpx"), 2048U + size.width * size.height * 6U);
	check_audio(out, five_one, frames);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_reels_test)
{
	auto film = new_test_film("dcdm_encoder_reels_test");
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_reels_test_src_a", 24));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_reels_test_src_b", 30));
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);
	BOOST_REQUIRE_EQUAL(film->reels()[0].duration().frames_round(24), 24);
	BOOST_REQUIRE_EQUAL(film->reels()[1].duration().frames_round(24), 30);

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);
	BOOST_CHECK_EQUAL(count_files(out / "reel_01", ".dpx"), 24);
	BOOST_CHECK_EQUAL(count_files(out / "reel_02", ".dpx"), 30);
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "frame_00000000.dpx"));
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "frame_00000029.dpx"));
	check_audio(out / "reel_01", five_one, 24);
	check_audio(out / "reel_02", five_one, 30);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_3d_test)
{
	auto film = new_test_film("dcdm_encoder_3d_test");
	auto content = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_3d_test_src", 6));
	content->video->set_frame_type(VideoFrameType::THREE_D_ALTERNATE);
	film->set_three_d(true);
	BOOST_REQUIRE(!wait_for_jobs());

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_3d_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);
	auto const frames = film->length().frames_round(24);
	BOOST_CHECK_EQUAL(count_files(out / "left", ".dpx"), frames);
	BOOST_CHECK_EQUAL(count_files(out / "right", ".dpx"), frames);
	check_audio(out, five_one, frames);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_rejects_low_quality_test)
{
	auto film = new_test_film("dcdm_encoder_rejects_low_quality_test", content_factory("test/data/10bit_video_422.mp4"));
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	try {
		static_cast<void>(make_shared<DCDMFilmEncoder>(film, job, "build/test/dcdm_encoder_rejects_low_quality_test_out"));
		BOOST_CHECK_MESSAGE(false, "no exception thrown");
	} catch (std::runtime_error& e) {
		BOOST_CHECK_MESSAGE(string(e.what()).find("10bit_video_422.mp4") != string::npos, e.what());
	}
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_nonempty_directory_test)
{
	auto film = new_test_film("dcdm_encoder_nonempty_directory_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_nonempty_directory_test_src", 2));

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_nonempty_directory_test_out");
	dcp::filesystem::remove_all(out);
	dcp::filesystem::create_directories(out);
	{
		std::ofstream f((out / "something").string());
		f << "x";
	}

	/* Refused when the encoder is made, before the player starts writing anything */
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_CHECK_THROW(static_cast<void>(make_shared<DCDMFilmEncoder>(film, job, out)), std::runtime_error);
	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);

	dcp::filesystem::remove_all(out);
}


/* --- Audio exactly as in the DCP, and Atmos --- */


static
vector<int32_t>
read_wav24(boost::filesystem::path path)
{
	std::ifstream in(path.string(), std::ios::binary);
	vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	vector<int32_t> samples;
	for (size_t i = 44; i + 2 < d.size(); i += 3) {
		int32_t v = int32_t(d[i]) | (int32_t(d[i + 1]) << 8) | (int32_t(d[i + 2]) << 16);
		if (v & 0x800000) {
			v -= 0x1000000;
		}
		samples.push_back(v);
	}
	return samples;
}


static
shared_ptr<dcp::CPL>
read_dcp(shared_ptr<Film> film)
{
	auto dcp = make_shared<dcp::DCP>(film->dir(film->dcp_name()));
	dcp->read();
	BOOST_REQUIRE_EQUAL(dcp->cpls().size(), 1U);
	return dcp->cpls()[0];
}


/** @return samples of one channel of one reel of a DCP's sound */
static
vector<int32_t>
dcp_sound(shared_ptr<dcp::CPL> cpl, int reel, int channel)
{
	auto sound = cpl->reels()[reel]->main_sound()->asset();
	auto reader = sound->start_read();
	vector<int32_t> samples;
	for (int64_t f = 0; f < sound->intrinsic_duration(); ++f) {
		auto frame = reader->get_frame(f);
		for (int s = 0; s < frame->samples(); ++s) {
			samples.push_back(frame->get(channel, s));
		}
	}
	return samples;
}


static
void
check_audio_matches_dcp(shared_ptr<Film> film, boost::filesystem::path out)
{
	auto cpl = read_dcp(film);
	int const reels = static_cast<int>(cpl->reels().size());
	for (int r = 0; r < reels; ++r) {
		auto const dir = reels == 1 ? out : out / fmt::format("reel_{:02d}", r + 1);
		int c = 0;
		for (auto const& label: five_one) {
			auto const dcdm = read_wav24(dir / "audio" / (label + ".wav"));
			auto const dcp = dcp_sound(cpl, r, c);
			BOOST_REQUIRE_EQUAL(dcdm.size(), dcp.size());
			bool same = true;
			for (size_t i = 0; i < dcp.size(); ++i) {
				if (dcdm[i] != dcp[i]) {
					BOOST_CHECK_MESSAGE(false, label << " sample " << i << ": " << dcdm[i] << " != " << dcp[i]);
					same = false;
					break;
				}
			}
			BOOST_CHECK_MESSAGE(same, "reel " << r << " channel " << label);
			++c;
		}
	}
}


BOOST_AUTO_TEST_CASE(dcdm_audio_gain_trim_test)
{
	auto film = new_test_film("dcdm_audio_gain_trim_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_audio_gain_trim_test_src", 24));
	auto sound = content_factory("test/data/L.wav")[0];
	film->examine_and_add_content({sound});
	BOOST_REQUIRE(!wait_for_jobs());
	sound->set_position(film, dcpomatic::DCPTime());
	sound->audio->set_gain(-6);
	sound->audio->set_delay(50);
	sound->set_trim_start(film, dcpomatic::ContentTime::from_seconds(0.1));

	auto const out = boost::filesystem::path("build/test/dcdm_audio_gain_trim_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	/* The source is not silent, so this would catch a lost signal too (a mono file goes to centre) */
	bool any_signal = false;
	for (auto const& label: five_one) {
		auto const samples = read_wav24(out / "audio" / (label + ".wav"));
		any_signal = any_signal || std::any_of(samples.begin(), samples.end(), [](int32_t s) { return s != 0; });
	}
	BOOST_CHECK(any_signal);
	check_audio_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_audio_processor_test)
{
	auto film = new_test_film("dcdm_audio_processor_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_audio_processor_test_src", 24));
	auto left = content_factory("test/data/L.wav")[0];
	auto right = content_factory("test/data/R.wav")[0];
	film->examine_and_add_content({left, right});
	BOOST_REQUIRE(!wait_for_jobs());
	left->set_position(film, dcpomatic::DCPTime());
	right->set_position(film, dcpomatic::DCPTime());
	film->set_audio_processor(AudioProcessor::from_id("mid-side-decoder"));

	auto const out = boost::filesystem::path("build/test/dcdm_audio_processor_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	check_audio_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}


/** @return the frames of an Atmos asset */
static
vector<vector<uint8_t>>
atmos_frames(shared_ptr<dcp::AtmosAsset> asset)
{
	vector<vector<uint8_t>> frames;
	auto reader = asset->start_read();
	for (int64_t f = 0; f < asset->intrinsic_duration(); ++f) {
		auto frame = reader->get_frame(f);
		frames.push_back(vector<uint8_t>(frame->data(), frame->data() + frame->size()));
	}
	return frames;
}


static
void
check_atmos_matches_dcp(shared_ptr<Film> film, boost::filesystem::path out)
{
	auto cpl = read_dcp(film);
	int const reels = static_cast<int>(cpl->reels().size());
	for (int r = 0; r < reels; ++r) {
		auto const path = (reels == 1 ? out : out / fmt::format("reel_{:02d}", r + 1)) / "atmos.mxf";
		auto dcp_atmos = cpl->reels()[r]->atmos();
		if (!dcp_atmos) {
			BOOST_CHECK_MESSAGE(!dcp::filesystem::exists(path), path.string());
			continue;
		}
		BOOST_REQUIRE_MESSAGE(dcp::filesystem::exists(path), path.string());
		auto const dcdm = atmos_frames(make_shared<dcp::AtmosAsset>(path));
		/* The DCP may reference the whole Atmos file and cut it with the CPL's entry point and duration */
		auto const all = atmos_frames(dcp_atmos->asset());
		auto const first = static_cast<size_t>(dcp_atmos->entry_point().get_value_or(0));
		auto const length = static_cast<size_t>(dcp_atmos->duration().get_value_or(dcp_atmos->actual_duration()));
		BOOST_REQUIRE_LE(first + length, all.size());
		vector<vector<uint8_t>> const dcp(all.begin() + first, all.begin() + first + length);
		BOOST_CHECK_EQUAL(dcdm.size(), dcp.size());
		BOOST_CHECK_MESSAGE(dcdm == dcp, "Atmos frames differ in reel " << r);
	}
}


BOOST_AUTO_TEST_CASE(dcdm_atmos_single_reel_test)
{
	auto film = new_test_film("dcdm_atmos_single_reel_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_atmos_single_reel_test_src", 24));
	auto atmos = content_factory("test/data/atmos_0.mxf")[0];
	film->examine_and_add_content({atmos});
	BOOST_REQUIRE(!wait_for_jobs());
	atmos->set_position(film, dcpomatic::DCPTime());

	auto const out = boost::filesystem::path("build/test/dcdm_atmos_single_reel_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK(dcp::filesystem::exists(out / "atmos.mxf"));
	check_atmos_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_atmos_spanning_reels_test)
{
	auto film = new_test_film("dcdm_atmos_spanning_reels_test");
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_atmos_spanning_reels_test_src_a", 24));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_atmos_spanning_reels_test_src_b", 24));
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	auto atmos = content_factory("test/data/atmos_0.mxf")[0];
	film->examine_and_add_content({atmos});
	BOOST_REQUIRE(!wait_for_jobs());
	/* Start the Atmos half way through reel 1 so that it runs into reel 2 */
	atmos->set_position(film, dcpomatic::DCPTime::from_frames(12, 24));
	BOOST_REQUIRE(atmos->end(film) > dcpomatic::DCPTime::from_frames(24, 24));
	/* ...but not past the end of the picture */
	auto const end = dcpomatic::DCPTime::from_frames(48, 24);
	if (atmos->end(film) > end) {
		atmos->set_trim_end(dcpomatic::ContentTime::from_frames((atmos->end(film) - end).frames_round(24), 24));
	}
	BOOST_REQUIRE(atmos->end(film) <= end);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);

	auto const out = boost::filesystem::path("build/test/dcdm_atmos_spanning_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	/* An Atmos asset that covers only part of a reel is short and differs in length from the
	 * picture; that is how DCP-o-matic makes such a DCP, and it is what we compare against.
	 */
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_DURATION,
			dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION,
			dcp::VerificationNote::Code::MISMATCHED_ASSET_DURATION
		});

	BOOST_CHECK(dcp::filesystem::exists(out / "reel_01" / "atmos.mxf"));
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "atmos.mxf"));
	check_atmos_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_atmos_trimmed_test)
{
	auto film = new_test_film("dcdm_atmos_trimmed_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_atmos_trimmed_test_src", 24));
	auto atmos = content_factory("test/data/atmos_0.mxf")[0];
	film->examine_and_add_content({atmos});
	BOOST_REQUIRE(!wait_for_jobs());
	atmos->set_position(film, dcpomatic::DCPTime());
	atmos->set_trim_start(film, dcpomatic::ContentTime::from_frames(5, 24));

	auto const out = boost::filesystem::path("build/test/dcdm_atmos_trimmed_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	check_atmos_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_no_atmos_test)
{
	auto film = new_test_film("dcdm_no_atmos_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_no_atmos_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_no_atmos_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	BOOST_CHECK(!dcp::filesystem::exists(out / "atmos.mxf"));
	dcp::filesystem::remove_all(out);
}


/* --- Open subtitles and closed captions --- */


struct SimpleText
{
	string text;
	int64_t in_ms;
	int64_t out_ms;

	bool operator==(SimpleText const& other) const {
		return text == other.text && in_ms == other.in_ms && out_ms == other.out_ms;
	}

	bool operator!=(SimpleText const& other) const {
		return !(*this == other);
	}
};


static
std::ostream&
operator<<(std::ostream& s, SimpleText const& t)
{
	return s << "\"" << t.text << "\" " << t.in_ms << "-" << t.out_ms;
}


static
vector<SimpleText>
simple_texts(shared_ptr<const dcp::TextAsset> asset)
{
	vector<SimpleText> out;
	for (auto t: asset->texts()) {
		auto s = std::dynamic_pointer_cast<const dcp::TextString>(t);
		/* Skip the blank texts that DCP-o-matic puts in a DCP's otherwise-empty subtitle assets */
		if (s && s->text().find_first_not_of(" \t") != string::npos) {
			out.push_back({ s->text(), static_cast<int64_t>(llrint(s->in().as_seconds() * 1000)), static_cast<int64_t>(llrint(s->out().as_seconds() * 1000)) });
		}
	}
	return out;
}


static
shared_ptr<dcp::TextAsset>
read_text_xml(shared_ptr<Film> film, boost::filesystem::path path)
{
	if (film->interop()) {
		return make_shared<dcp::InteropTextAsset>(path);
	}
	return make_shared<dcp::SMPTETextAsset>(path);
}


/** Add subtitles at @param position, trimmed so that they end at @param end
 *  (the file's subtitles run for nearly 6 seconds, and we want to keep the films short).
 */
static
shared_ptr<Content>
add_subtitles(shared_ptr<Film> film, string file, TextType type, dcpomatic::DCPTime position, dcpomatic::DCPTime end)
{
	auto subs = content_factory("test/data/" + file)[0];
	film->examine_and_add_content({subs});
	BOOST_REQUIRE(!wait_for_jobs());
	subs->set_position(film, position);
	subs->set_trim_end(dcpomatic::ContentTime::from_seconds(subs->full_length(film).seconds() - (end - position).seconds()));
	BOOST_REQUIRE(subs->end(film) <= end);
	subs->only_text()->set_use(true);
	subs->only_text()->set_burn(false);
	subs->only_text()->set_type(type);
	if (type == TextType::CLOSED_CAPTION) {
		subs->only_text()->set_dcp_track(DCPTextTrack("English", dcp::LanguageTag("en-GB")));
	}
	return subs;
}


BOOST_AUTO_TEST_CASE(dcdm_open_subtitles_test)
{
	auto film = new_test_film("dcdm_open_subtitles_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_open_subtitles_test_src", 24));
	add_subtitles(film, "dcp_sub3.xml", TextType::OPEN_SUBTITLE, dcpomatic::DCPTime(), dcpomatic::DCPTime::from_frames(24, 24));

	auto const out = boost::filesystem::path("build/test/dcdm_open_subtitles_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME, dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE, dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION, dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21 });

	auto const path = out / "subtitles" / "subtitles.xml";
	BOOST_REQUIRE_MESSAGE(dcp::filesystem::exists(path), path.string());
	auto const dcdm = simple_texts(read_text_xml(film, path));
	auto const dcp = simple_texts(read_dcp(film)->reels()[0]->main_subtitle()->asset());
	BOOST_REQUIRE(!dcp.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(dcdm.begin(), dcdm.end(), dcp.begin(), dcp.end());
	BOOST_CHECK(!dcp::filesystem::exists(out / "captions"));

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_closed_captions_test)
{
	auto film = new_test_film("dcdm_closed_captions_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_closed_captions_test_src", 24));
	add_subtitles(film, "dcp_sub3.xml", TextType::CLOSED_CAPTION, dcpomatic::DCPTime(), dcpomatic::DCPTime::from_frames(24, 24));

	auto const out = boost::filesystem::path("build/test/dcdm_closed_captions_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME, dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE, dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION, dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21 });

	BOOST_REQUIRE_EQUAL(count_files(out / "captions", ".xml"), 1);
	boost::filesystem::path path;
	for (auto const& i: dcp::filesystem::directory_iterator(out / "captions")) {
		if (i.path().extension() == ".xml") {
			path = i.path();
		}
	}
	auto const dcdm = simple_texts(read_text_xml(film, path));
	auto const ccaps = read_dcp(film)->reels()[0]->closed_captions();
	BOOST_REQUIRE_EQUAL(ccaps.size(), 1U);
	auto const dcp = simple_texts(ccaps[0]->asset());
	BOOST_REQUIRE(!dcp.empty());
	BOOST_CHECK_EQUAL_COLLECTIONS(dcdm.begin(), dcdm.end(), dcp.begin(), dcp.end());
	BOOST_CHECK(!dcp::filesystem::exists(out / "subtitles"));

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_text_reels_test)
{
	auto film = new_test_film("dcdm_text_reels_test");
	/* Otherwise the playlist moves the subtitles back to the start */
	film->set_sequence(false);
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_text_reels_test_src_a", 24));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_text_reels_test_src_b", 24));
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	/* Subtitles only in reel 2, so their times must be relative to the reel */
	add_subtitles(film, "dcp_sub3.xml", TextType::OPEN_SUBTITLE, dcpomatic::DCPTime::from_frames(24, 24), dcpomatic::DCPTime::from_frames(48, 24));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);

	auto const out = boost::filesystem::path("build/test/dcdm_text_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE
		});

	auto cpl = read_dcp(film);
	for (int r = 0; r < 2; ++r) {
		auto const path = out / fmt::format("reel_{:02d}", r + 1) / "subtitles" / "subtitles.xml";
		auto dcp_sub = cpl->reels()[r]->main_subtitle();
		auto const dcp = dcp_sub ? simple_texts(dcp_sub->asset()) : vector<SimpleText>();
		auto const dcdm = dcp::filesystem::exists(path) ? simple_texts(read_text_xml(film, path)) : vector<SimpleText>();
		BOOST_CHECK_EQUAL_COLLECTIONS(dcdm.begin(), dcdm.end(), dcp.begin(), dcp.end());
	}
	BOOST_CHECK(!dcp::filesystem::exists(out / "reel_01" / "subtitles"));
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "subtitles" / "subtitles.xml"));

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_no_text_test)
{
	auto film = new_test_film("dcdm_no_text_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_no_text_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_no_text_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	BOOST_CHECK(!dcp::filesystem::exists(out / "subtitles"));
	BOOST_CHECK(!dcp::filesystem::exists(out / "captions"));
	dcp::filesystem::remove_all(out);
}


/* --- HI, VI and sign language are written when they are used --- */


BOOST_AUTO_TEST_CASE(dcdm_extra_channels_written_test)
{
	auto film = new_test_film("dcdm_extra_channels_written_test");
	film->set_audio_channels(16);
	dpx_content(film, make_dpx_sequence("build/test/dcdm_extra_channels_written_test_src", 24));

	vector<std::pair<dcp::Channel, string>> const extras = {
		{ dcp::Channel::HI, "HI" },
		{ dcp::Channel::VI, "VI" },
		{ dcp::Channel::SIGN_LANGUAGE, "SL" }
	};
	/* Lc and Rc are mapped too, but DCP-o-matic writes them silent in a DCP so they are not exported */
	auto mapped = extras;
	mapped.push_back({ dcp::Channel::LC, "Lc" });
	mapped.push_back({ dcp::Channel::RC, "Rc" });
	for (auto const& extra: mapped) {
		auto sound = content_factory("test/data/L.wav")[0];
		film->examine_and_add_content({sound});
		BOOST_REQUIRE(!wait_for_jobs());
		sound->set_position(film, dcpomatic::DCPTime());
		auto mapping = sound->audio->mapping();
		mapping.make_zero();
		mapping.set(0, extra.first, 1);
		sound->audio->set_mapping(mapping);
	}

	auto const out = boost::filesystem::path("build/test/dcdm_extra_channels_written_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	/* 5.1 is always there, plus one file for each extra channel in use */
	vector<string> labels = five_one;
	for (auto const& extra: extras) {
		labels.push_back(extra.second);
	}
	check_audio(out, labels, 24);
	BOOST_CHECK(!dcp::filesystem::exists(out / "audio" / "Lc.wav"));
	BOOST_CHECK(!dcp::filesystem::exists(out / "audio" / "Rc.wav"));

	/* The extras hold the DCP's samples for their channels */
	auto cpl = read_dcp(film);
	for (auto const& extra: extras) {
		auto const dcdm = read_wav24(out / "audio" / (extra.second + ".wav"));
		auto const dcp = dcp_sound(cpl, 0, static_cast<int>(extra.first));
		BOOST_REQUIRE_EQUAL(dcdm.size(), dcp.size());
		BOOST_CHECK_MESSAGE(dcdm == dcp, extra.second);
		BOOST_CHECK_MESSAGE(std::any_of(dcdm.begin(), dcdm.end(), [](int32_t s) { return s != 0; }), extra.second << " is silent");
	}

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_no_extra_channels_test)
{
	auto film = new_test_film("dcdm_no_extra_channels_test");
	film->set_audio_channels(16);
	dpx_content(film, make_dpx_sequence("build/test/dcdm_no_extra_channels_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_no_extra_channels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	check_audio(out, five_one, film->length().frames_round(24));
	dcp::filesystem::remove_all(out);
}


/* --- Lossless JPEG 2000 (SMPTE ST 428-24 packed images), the default --- */


static
vector<boost::filesystem::path>
files_with(boost::filesystem::path dir, string suffix)
{
	vector<boost::filesystem::path> out;
	if (!dcp::filesystem::is_directory(dir)) {
		return out;
	}
	for (auto const& i: dcp::filesystem::directory_iterator(dir)) {
		auto const name = i.path().filename().string();
		if (name.size() > suffix.size() && name.substr(name.size() - suffix.size()) == suffix) {
			out.push_back(i.path());
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_j2k_is_default_test)
{
	auto film = new_test_film("dcdm_encoder_j2k_is_default_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_j2k_is_default_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_encoder_j2k_is_default_test_out");
	dcp::filesystem::remove_all(out);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, out));
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());

	BOOST_CHECK_EQUAL(files_with(out / "picture", ".pdc.j2c").size(), 24U);
	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);
	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_j2k_matches_dpx_test)
{
	auto film = new_test_film("dcdm_encoder_j2k_matches_dpx_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_j2k_matches_dpx_test_src", 3));

	auto const j2k = boost::filesystem::path("build/test/dcdm_encoder_j2k_matches_dpx_test_j2k");
	auto const dpx = boost::filesystem::path("build/test/dcdm_encoder_j2k_matches_dpx_test_dpx");
	dcp::filesystem::remove_all(j2k);
	dcp::filesystem::remove_all(dpx);
	run_export(film, j2k, DCDMPictureFormat::J2K_LOSSLESS);
	run_export(film, dpx, DCDMPictureFormat::DPX);

	/* ST 428-24 names: contiguous, fixed-width frame numbers and the .pdc.j2c suffix */
	auto const frames = files_with(j2k / "picture", ".pdc.j2c");
	BOOST_REQUIRE_EQUAL(frames.size(), 24U);
	BOOST_CHECK_EQUAL(frames[0].filename().string(), "00000000.pdc.j2c");
	BOOST_CHECK_EQUAL(frames[23].filename().string(), "00000023.pdc.j2c");

	/* Every pixel of every frame decodes to exactly the DPX's 12-bit values */
	for (int f = 0; f < 24; ++f) {
		std::ifstream in(frames[f].string(), std::ios::binary);
		vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		auto const decoded = dcp::decompress_j2k(data.data(), static_cast<int64_t>(data.size()), 0);

		std::ifstream dpx_in((dpx / fmt::format("frame_{:08d}.dpx", f)).string(), std::ios::binary);
		vector<uint8_t> d((std::istreambuf_iterator<char>(dpx_in)), std::istreambuf_iterator<char>());
		auto const size = decoded->size();
		BOOST_REQUIRE_EQUAL(d.size(), 2048U + size.width * size.height * 6U);
		int64_t differences = 0;
		for (int i = 0; i < size.width * size.height; ++i) {
			for (int c = 0; c < 3; ++c) {
				size_t const o = 2048 + (i * 3 + c) * 2;
				int const value = ((d[o] << 8) | d[o + 1]) >> 4;
				if (value != decoded->data(c)[i]) {
					++differences;
				}
			}
		}
		BOOST_CHECK_MESSAGE(differences == 0, "frame " << f << ": " << differences << " values differ");
	}

	dcp::filesystem::remove_all(j2k);
	dcp::filesystem::remove_all(dpx);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_j2k_3d_reels_test)
{
	auto film = new_test_film("dcdm_encoder_j2k_3d_reels_test");
	film->set_sequence(false);
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_j2k_3d_reels_test_src_a", 48));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_j2k_3d_reels_test_src_b", 48));
	a->video->set_frame_type(VideoFrameType::THREE_D_ALTERNATE);
	b->video->set_frame_type(VideoFrameType::THREE_D_ALTERNATE);
	film->set_three_d(true);
	BOOST_REQUIRE(!wait_for_jobs());
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_j2k_3d_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out, DCDMPictureFormat::J2K_LOSSLESS);

	for (auto reel: { "reel_01", "reel_02" }) {
		/* Each eye is its own sequence in its own directory */
		BOOST_CHECK_EQUAL(files_with(out / reel / "picture" / "left", ".pdc.j2c").size(), 24U);
		BOOST_CHECK_EQUAL(files_with(out / reel / "picture" / "right", ".pdc.j2c").size(), 24U);
		BOOST_CHECK(dcp::filesystem::exists(out / reel / "picture" / "left" / "00000000.pdc.j2c"));
	}

	dcp::filesystem::remove_all(out);
}


/* --- DCDM_INFO.txt describes the export and the standards it follows --- */


static
string
read_text(boost::filesystem::path path)
{
	std::ifstream in(path.string());
	return string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}


BOOST_AUTO_TEST_CASE(dcdm_info_file_j2k_test)
{
	auto film = new_test_film("dcdm_info_file_j2k_test");
	film->set_name("My Film");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_info_file_j2k_test_src", 2));
	add_subtitles(film, "dcp_sub3.xml", TextType::OPEN_SUBTITLE, dcpomatic::DCPTime(), dcpomatic::DCPTime::from_frames(24, 24));

	auto const out = boost::filesystem::path("build/test/dcdm_info_file_j2k_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out, DCDMPictureFormat::J2K_LOSSLESS);

	auto const path = out / "DCDM_INFO.txt";
	BOOST_REQUIRE(dcp::filesystem::exists(path));
	auto const text = read_text(path);
	for (auto expected: {
		"My Film",
		"Lossless JPEG 2000",
		"SMPTE ST 428-24",
		"SMPTE ST 428-1",
		"SMPTE ST 2067-40",
		"Re-encode JPEG2000 data",
		"ISO/IEC 15444-1",
		"1998x1080",
		"24 fps",
		"2D",
		"Reels: 1",
		"5.1",
		"L, R, C, LFE, Ls, Rs",
		"24-bit",
		"48000 Hz",
		"SMPTE ST 428-2",
		"SMPTE ST 428-12",
		"Open subtitles: yes",
		"SMPTE ST 428-7",
		"Atmos: no"
		}) {
		BOOST_CHECK_MESSAGE(text.find(expected) != string::npos, "missing: " << expected);
	}

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_info_file_dpx_test)
{
	auto film = new_test_film("dcdm_info_file_dpx_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_info_file_dpx_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_info_file_dpx_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out, DCDMPictureFormat::DPX);

	auto const text = read_text(out / "DCDM_INFO.txt");
	BOOST_CHECK(text.find("DPX") != string::npos);
	BOOST_CHECK(text.find("SMPTE ST 268") != string::npos);
	BOOST_CHECK(text.find("SMPTE ST 428-24") == string::npos);

	dcp::filesystem::remove_all(out);
}


/* --- Review fixes --- */


/** A subtitle that crosses a reel boundary is split exactly as the DCP splits it */
BOOST_AUTO_TEST_CASE(dcdm_subtitle_across_reels_test)
{
	auto film = new_test_film("dcdm_subtitle_across_reels_test");
	film->set_sequence(false);
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_subtitle_across_reels_test_src_a", 24));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_subtitle_across_reels_test_src_b", 24));
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	/* The first subtitle starts at 20 frames and runs past the boundary at 24 */
	add_subtitles(film, "dcp_sub3.xml", TextType::OPEN_SUBTITLE, dcpomatic::DCPTime(), dcpomatic::DCPTime::from_frames(48, 24));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);

	auto const out = boost::filesystem::path("build/test/dcdm_subtitle_across_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE
		});

	auto cpl = read_dcp(film);
	for (int r = 0; r < 2; ++r) {
		auto const path = out / fmt::format("reel_{:02d}", r + 1) / "subtitles" / "subtitles.xml";
		auto dcp_sub = cpl->reels()[r]->main_subtitle();
		auto const dcp = dcp_sub ? simple_texts(dcp_sub->asset()) : vector<SimpleText>();
		auto const dcdm = dcp::filesystem::exists(path) ? simple_texts(read_text_xml(film, path)) : vector<SimpleText>();
		BOOST_REQUIRE_MESSAGE(!dcp.empty(), "reel " << r << " of the DCP has no subtitles");
		BOOST_CHECK_EQUAL_COLLECTIONS(dcdm.begin(), dcdm.end(), dcp.begin(), dcp.end());
	}

	dcp::filesystem::remove_all(out);
}


/** An export that fails part-way marks the folder as incomplete, finalises what it can,
 *  and a later export to the same folder says why it is refused.
 */
BOOST_AUTO_TEST_CASE(dcdm_failed_export_marked_incomplete_test)
{
	auto film = new_test_film("dcdm_failed_export_marked_incomplete_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_failed_export_marked_incomplete_test_src", 2));

	auto const out = boost::filesystem::path("build/test/dcdm_failed_export_marked_incomplete_test_out");
	dcp::filesystem::remove_all(out);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto encoder = make_shared<DCDMFilmEncoder>(film, job, out, DCDMPictureFormat::DPX);
	/* Put a directory where frame 5 should go so that writing it fails */
	dcp::filesystem::create_directories(out / "frame_00000005.dpx");
	BOOST_CHECK_THROW(encoder->go(), std::exception);
	encoder.reset();

	BOOST_CHECK(dcp::filesystem::exists(out / "DCDM_INCOMPLETE.txt"));
	BOOST_CHECK(!dcp::filesystem::exists(out / "DCDM_INFO.txt"));
	/* The WAVs that were started have valid headers covering what was written */
	auto const size = dcp::filesystem::file_size(out / "audio" / "L.wav");
	BOOST_CHECK_EQUAL((size - 44) % 3, 0U);
	std::ifstream wav((out / "audio" / "L.wav").string(), std::ios::binary);
	vector<uint8_t> header(44);
	wav.read(reinterpret_cast<char*>(header.data()), 44);
	uint32_t const data_size = header[40] | (header[41] << 8) | (header[42] << 16) | (header[43] << 24);
	BOOST_CHECK_EQUAL(data_size, size - 44);

	auto job2 = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	try {
		static_cast<void>(make_shared<DCDMFilmEncoder>(film, job2, out));
		BOOST_CHECK_MESSAGE(false, "no exception thrown");
	} catch (std::runtime_error& e) {
		BOOST_CHECK_MESSAGE(string(e.what()).find("incomplete DCDM export") != string::npos, e.what());
	}

	dcp::filesystem::remove_all(out);
}


/** Bitmap (image) subtitles are exported as in the DCP, and DCDM_INFO.txt reports them */
BOOST_AUTO_TEST_CASE(dcdm_bitmap_subtitles_test)
{
	auto film = new_test_film("dcdm_bitmap_subtitles_test");
	film->set_interop(true);
	dpx_content(film, make_dpx_sequence("build/test/dcdm_bitmap_subtitles_test_src", 24 * 6));
	auto subs = content_factory("test/data/png_subs/subs.xml")[0];
	film->examine_and_add_content({subs});
	BOOST_REQUIRE(!wait_for_jobs());
	subs->only_text()->set_use(true);
	subs->only_text()->set_burn(false);

	auto const out = boost::filesystem::path("build/test/dcdm_bitmap_subtitles_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	auto const path = out / "subtitles" / "subtitles.xml";
	BOOST_REQUIRE_MESSAGE(dcp::filesystem::exists(path), path.string());
	auto asset = make_shared<dcp::InteropTextAsset>(path);
	int images = 0;
	for (auto t: asset->texts()) {
		if (std::dynamic_pointer_cast<const dcp::TextImage>(t)) {
			++images;
		}
	}
	BOOST_CHECK_EQUAL(images, 1);
	BOOST_CHECK_EQUAL(count_files(out / "subtitles", ".png"), 1);
	auto const info = read_text(out / "DCDM_INFO.txt");
	BOOST_CHECK(info.find("Open subtitles: yes") != string::npos);

	dcp::filesystem::remove_all(out);
}


/** DCDM_INFO.txt only claims subtitles that were actually written */
BOOST_AUTO_TEST_CASE(dcdm_info_reports_written_text_test)
{
	auto film = new_test_film("dcdm_info_reports_written_text_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_info_reports_written_text_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_info_reports_written_text_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	auto const info = read_text(out / "DCDM_INFO.txt");
	BOOST_CHECK(info.find("Open subtitles: no") != string::npos);
	BOOST_CHECK(info.find("Closed captions: no") != string::npos);
	dcp::filesystem::remove_all(out);
}


/* --- Minor review fixes --- */


/** DCPB: a DPX DCDM says how to import it (no colour conversion) */
BOOST_AUTO_TEST_CASE(dcdm_info_dpx_import_note_test)
{
	auto film = new_test_film("dcdm_info_dpx_import_note_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_info_dpx_import_note_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_info_dpx_import_note_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out, DCDMPictureFormat::DPX);
	auto const text = read_text(out / "DCDM_INFO.txt");
	BOOST_CHECK(text.find("colour conversion") != string::npos);
	BOOST_CHECK(text.find("None") != string::npos);
	dcp::filesystem::remove_all(out);
}


/** M6: content whose video is not used does not block the export */
BOOST_AUTO_TEST_CASE(dcdm_gate_ignores_unused_video_test)
{
	auto film = new_test_film("dcdm_gate_ignores_unused_video_test");
	auto good = dpx_content(film, make_dpx_sequence("build/test/dcdm_gate_ignores_unused_video_test_src", 2));
	auto bad = content_factory("test/data/10bit_video_422.mp4")[0];
	film->examine_and_add_content({bad});
	BOOST_REQUIRE(!wait_for_jobs());
	bad->video->set_use(false);
	auto const result = dcdm_source_quality(film);
	BOOST_CHECK_MESSAGE(result.ok, result.reason);
	(void) good;
}


/** M2: a late Atmos frame after the export has finished is ignored, not a crash */
struct dcdm_atmos_after_finish_is_ignored_test;
BOOST_AUTO_TEST_CASE(dcdm_atmos_after_finish_is_ignored_test)
{
	auto film = new_test_film("dcdm_atmos_after_finish_is_ignored_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_atmos_after_finish_is_ignored_test_src", 2));
	auto const out = boost::filesystem::path("build/test/dcdm_atmos_after_finish_is_ignored_test_out");
	dcp::filesystem::remove_all(out);
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto encoder = make_shared<DCDMFilmEncoder>(film, job, out, DCDMPictureFormat::DPX);
	encoder->go();
	/* Finished: a stray frame must now be a no-op, not a null dereference */
	BOOST_CHECK_NO_THROW(encoder->atmos(nullptr, dcpomatic::DCPTime(), AtmosMetadata(make_shared<dcp::AtmosAsset>(boost::filesystem::path("test/data/atmos_0.mxf")))));
	dcp::filesystem::remove_all(out);
}


/** Source PCM at 44.1 kHz comes out at the film's 48 kHz, with its duration kept */
BOOST_AUTO_TEST_CASE(dcdm_pcm_resample_44k_test)
{
	auto film = new_test_film("dcdm_pcm_resample_44k_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_pcm_resample_44k_test_src", 24));

	auto const wav = boost::filesystem::path("build/test/dcdm_pcm_resample_44k_test.wav");
	{
		WAVWriter writer(wav, 44100);
		vector<float> tone(44100);
		for (size_t i = 0; i < tone.size(); ++i) {
			tone[i] = 0.5f * std::sin(2 * M_PI * 1000 * i / 44100.0);
		}
		writer.write(tone.data(), tone.size());
		writer.close();
	}
	auto sound = content_factory(wav)[0];
	film->examine_and_add_content({sound});
	BOOST_REQUIRE(!wait_for_jobs());
	sound->set_position(film, dcpomatic::DCPTime());

	auto const out = boost::filesystem::path("build/test/dcdm_pcm_resample_44k_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	/* A mono file goes to the centre channel */
	auto const samples = read_wav24(out / "audio" / "C.wav");
	/* One second in, one second out: 48000 samples, within a few samples of resampler delay */
	BOOST_CHECK_MESSAGE(std::abs(static_cast<long>(samples.size()) - 48000L) <= 96, samples.size());
	BOOST_CHECK(std::any_of(samples.begin(), samples.end(), [](int32_t s) { return s != 0; }));

	dcp::filesystem::remove_all(out);
}


/** A cancelled export is marked incomplete (DCDM_INCOMPLETE.txt) and has no DCDM_INFO.txt */
BOOST_AUTO_TEST_CASE(dcdm_cancelled_export_marked_incomplete_test)
{
	auto film = new_test_film("dcdm_cancelled_export_marked_incomplete_test");
	/* Long enough that the export is still running when it is cancelled */
	dpx_content(film, make_dpx_sequence("build/test/dcdm_cancelled_export_marked_incomplete_test_src", 24 * 20));

	auto const out = boost::filesystem::path("build/test/dcdm_cancelled_export_marked_incomplete_test_out");
	dcp::filesystem::remove_all(out);
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, out, DCDMPictureFormat::DPX));
	JobManager::instance()->add(job);

	/* Wait until pictures are being written, then cancel */
	for (int i = 0; i < 100 && count_files(out, ".dpx") < 3; ++i) {
		dcpomatic_sleep_seconds(1);
	}
	BOOST_REQUIRE(count_files(out, ".dpx") >= 3);
	job->cancel();
	wait_for_jobs();

	BOOST_CHECK(job->finished_cancelled());
	BOOST_CHECK(dcp::filesystem::exists(out / "DCDM_INCOMPLETE.txt"));
	BOOST_CHECK(!dcp::filesystem::exists(out / "DCDM_INFO.txt"));
	dcp::filesystem::remove_all(out);
}


/** Standalone WAV and audio embedded in a ProRes video file both end up in the DCDM,
 *  with the same samples as the DCP made from the same film.
 */
BOOST_AUTO_TEST_CASE(dcdm_wav_and_embedded_audio_test)
{
	/* A ProRes HQ MOV with its own audio, made by DCP-o-matic itself */
	auto const mov = boost::filesystem::path("build/test/dcdm_wav_and_embedded_audio_test_source.mov");
	{
		auto source = new_test_film("dcdm_wav_and_embedded_audio_test_source");
		dpx_content(source, make_dpx_sequence("build/test/dcdm_wav_and_embedded_audio_test_src", 24));
		auto tone = content_factory("test/data/L.wav")[0];
		source->examine_and_add_content({tone});
		BOOST_REQUIRE(!wait_for_jobs());
		tone->set_position(source, dcpomatic::DCPTime());
		dcp::filesystem::remove(mov);
		auto job = make_shared<TranscodeJob>(source, TranscodeJob::ChangedBehaviour::IGNORE);
		job->set_encoder(make_shared<FFmpegFilmEncoder>(source, job, mov, ExportFormat::PRORES_HQ, false, false, false, 23));
		JobManager::instance()->add(job);
		BOOST_REQUIRE(!wait_for_jobs());
	}

	auto film = new_test_film("dcdm_wav_and_embedded_audio_test");
	film->set_sequence(false);
	auto picture = dpx_content(film, make_dpx_sequence("build/test/dcdm_wav_and_embedded_audio_test_pic", 24));
	auto video = content_factory(mov)[0];
	film->examine_and_add_content({video});
	BOOST_REQUIRE(!wait_for_jobs());
	video->set_position(film, dcpomatic::DCPTime());
	/* A standalone WAV on the right channel, and the embedded audio on the left */
	auto wav = content_factory("test/data/R.wav")[0];
	film->examine_and_add_content({wav});
	BOOST_REQUIRE(!wait_for_jobs());
	wav->set_position(film, dcpomatic::DCPTime());
	(void) picture;

	auto const out = boost::filesystem::path("build/test/dcdm_wav_and_embedded_audio_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_DURATION,
			dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION,
			dcp::VerificationNote::Code::MISMATCHED_ASSET_DURATION
		});

	check_audio(out, five_one, film->length().frames_round(24));
	check_audio_matches_dcp(film, out);

	dcp::filesystem::remove_all(out);
}
