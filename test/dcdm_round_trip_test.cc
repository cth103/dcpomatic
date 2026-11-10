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


/** @file  test/dcdm_round_trip_test.cc
 *  @brief The DCDM promise: importing an exported DCDM and making a DCP gives the same DCP.
 *  @ingroup feature
 */


#include "lib/atmos_mxf_content.h"
#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcdm_film_encoder.h"
#include "lib/dcdm_import.h"
#include "lib/dcp_text_track.h"
#include "lib/dpx_writer.h"
#include "lib/film.h"
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
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/mono_j2k_picture_asset_reader.h>
#include <dcp/mono_j2k_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <dcp/text_asset.h>
#include <dcp/text_string.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;


static
boost::filesystem::path
make_dpx_sequence(boost::filesystem::path dir, int frames)
{
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(64, 48), Image::Alignment::COMPACT);
	for (int i = 0; i < frames; ++i) {
		/* A gradient that changes each frame, so that picture mistakes show */
		for (int y = 0; y < 48; ++y) {
			auto row = reinterpret_cast<uint16_t*>(image->data()[0] + y * image->stride()[0]);
			for (int x = 0; x < 64 * 3; ++x) {
				row[x] = static_cast<uint16_t>(((x * 20 + y * 50 + i * 100) % 4096) << 4);
			}
		}
		write_dpx(dir / fmt::format("src_{:04d}.dpx", i), image);
	}
	return dir;
}


static
shared_ptr<Content>
add(shared_ptr<Film> film, boost::filesystem::path path, DCPTime position)
{
	auto content = content_factory(path)[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE(!wait_for_jobs());
	content->set_position(film, position);
	return content;
}


static
void
export_dcdm(shared_ptr<Film> film, boost::filesystem::path out, DCDMPictureFormat format = DCDMPictureFormat::DPX)
{
	dcp::filesystem::remove_all(out);
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, out, format));
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());
}


static
vector<dcp::VerificationNote::Code>
ignore()
{
	return {
		dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
		dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
		dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21,
		dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME,
		dcp::VerificationNote::Code::INVALID_DURATION,
		dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION,
		dcp::VerificationNote::Code::MISMATCHED_ASSET_DURATION
	};
}


/** Make the film that re-imports a DCDM with DCDMImport, as adding its folder does in the program */
static
shared_ptr<Film>
reimport(string name, boost::filesystem::path dcdm)
{
	auto film = new_test_film(name);
	BOOST_REQUIRE(DCDMImport::valid(dcdm));
	DCDMImport(dcdm).examine_and_add(film);
	BOOST_REQUIRE(!wait_for_jobs());
	return film;
}


static
shared_ptr<dcp::CPL>
cpl_of(shared_ptr<Film> film)
{
	auto dcp = make_shared<dcp::DCP>(film->dir(film->dcp_name()));
	dcp->read();
	BOOST_REQUIRE_EQUAL(dcp->cpls().size(), 1U);
	return dcp->cpls()[0];
}


static
void
check_same_dcp(shared_ptr<Film> a_film, shared_ptr<Film> b_film)
{
	auto a = cpl_of(a_film);
	auto b = cpl_of(b_film);
	BOOST_REQUIRE_EQUAL(a->reels().size(), b->reels().size());

	for (size_t r = 0; r < a->reels().size(); ++r) {
		auto ra = a->reels()[r];
		auto rb = b->reels()[r];
		BOOST_CHECK_EQUAL(ra->duration(), rb->duration());

		/* Picture: decoded XYZ must be identical */
		auto pa = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(ra->main_picture()->asset());
		auto pb = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(rb->main_picture()->asset());
		BOOST_REQUIRE(pa && pb);
		BOOST_REQUIRE_EQUAL(pa->intrinsic_duration(), pb->intrinsic_duration());
		auto read_a = pa->start_read();
		auto read_b = pb->start_read();
		int64_t bytes_differ = 0;
		int max_error = 0;
		for (int64_t f = 0; f < pa->intrinsic_duration(); ++f) {
			auto fa = read_a->get_frame(f);
			auto fb = read_b->get_frame(f);
			if (fa->size() == fb->size() && memcmp(fa->data(), fb->data(), fa->size()) == 0) {
				continue;
			}
			++bytes_differ;
			auto xa = fa->xyz_image();
			auto xb = fb->xyz_image();
			auto const size = xa->size();
			for (int c = 0; c < 3; ++c) {
				for (int i = 0; i < size.width * size.height; ++i) {
					max_error = std::max(max_error, std::abs(xa->data(c)[i] - xb->data(c)[i]));
				}
			}
		}
		BOOST_CHECK_MESSAGE(max_error == 0, "reel " << r << ": " << bytes_differ << " J2K frames differ, max XYZ error " << max_error);

		/* Sound: every sample identical */
		auto sa = ra->main_sound()->asset();
		auto sb = rb->main_sound()->asset();
		BOOST_REQUIRE_EQUAL(sa->intrinsic_duration(), sb->intrinsic_duration());
		auto sread_a = sa->start_read();
		auto sread_b = sb->start_read();
		int64_t samples_differ = 0;
		for (int64_t f = 0; f < sa->intrinsic_duration(); ++f) {
			auto fa = sread_a->get_frame(f);
			auto fb = sread_b->get_frame(f);
			for (int c = 0; c < std::min(sa->channels(), sb->channels()); ++c) {
				/* The Atmos sync track encodes the Atmos asset's UUID, which is different in every DCP */
				if (c == static_cast<int>(dcp::Channel::SYNC_SIGNAL)) {
					continue;
				}
				for (int s = 0; s < fa->samples(); ++s) {
					if (fa->get(c, s) != fb->get(c, s)) {
						++samples_differ;
					}
				}
			}
		}
		BOOST_CHECK_MESSAGE(samples_differ == 0, "reel " << r << ": " << samples_differ << " audio samples differ");

		/* Atmos: same frames */
		BOOST_CHECK_EQUAL(static_cast<bool>(ra->atmos()), static_cast<bool>(rb->atmos()));
		if (ra->atmos() && rb->atmos()) {
			auto aa = ra->atmos()->asset();
			auto ab = rb->atmos()->asset();
			BOOST_REQUIRE_EQUAL(aa->intrinsic_duration(), ab->intrinsic_duration());
			auto aread = aa->start_read();
			auto bread = ab->start_read();
			for (int64_t f = 0; f < aa->intrinsic_duration(); ++f) {
				auto fa = aread->get_frame(f);
				auto fb = bread->get_frame(f);
				BOOST_REQUIRE(fa->size() == fb->size() && memcmp(fa->data(), fb->data(), fa->size()) == 0);
			}
		}

		/* Subtitles and closed captions: same non-blank text and times */
		auto texts = [](shared_ptr<dcp::ReelTextAsset> text) {
			vector<string> out;
			if (text) {
				for (auto t: text->asset()->texts()) {
					auto s = dynamic_pointer_cast<const dcp::TextString>(t);
					if (s && s->text().find_first_not_of(" \t") != string::npos) {
						out.push_back(fmt::format("{} {} {}", s->text(), s->in().as_string(dcp::Standard::SMPTE), s->out().as_string(dcp::Standard::SMPTE)));
					}
				}
			}
			return out;
		};
		auto const ta = texts(ra->main_subtitle());
		auto const tb = texts(rb->main_subtitle());
		BOOST_CHECK_EQUAL_COLLECTIONS(ta.begin(), ta.end(), tb.begin(), tb.end());

		BOOST_REQUIRE_EQUAL(ra->closed_captions().size(), rb->closed_captions().size());
		for (size_t c = 0; c < ra->closed_captions().size(); ++c) {
			BOOST_CHECK_EQUAL(ra->closed_captions()[c]->annotation_text().get_value_or(""), rb->closed_captions()[c]->annotation_text().get_value_or(""));
			BOOST_CHECK_EQUAL(ra->closed_captions()[c]->language().get_value_or(""), rb->closed_captions()[c]->language().get_value_or(""));
			auto const ca = texts(ra->closed_captions()[c]);
			auto const cb = texts(rb->closed_captions()[c]);
			BOOST_CHECK_EQUAL_COLLECTIONS(ca.begin(), ca.end(), cb.begin(), cb.end());
		}
	}
}


BOOST_AUTO_TEST_CASE(dcdm_round_trip_picture_audio_test)
{
	auto a = new_test_film("dcdm_round_trip_picture_audio_test_a");
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_picture_audio_test_src", 24), DCPTime());
	auto sound = add(a, "test/data/L.wav", DCPTime());
	sound->audio->set_gain(-6);
	sound->audio->set_delay(50);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_picture_audio_test_dcdm");
	export_dcdm(a, out);

	auto b = reimport("dcdm_round_trip_picture_audio_test_b", out);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_round_trip_71_test)
{
	auto a = new_test_film("dcdm_round_trip_71_test_a");
	a->set_audio_channels(12);
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_71_test_src", 24), DCPTime());
	auto rear = add(a, "test/data/L.wav", DCPTime());
	auto mapping = rear->audio->mapping();
	mapping.make_zero();
	mapping.set(0, dcp::Channel::BSL, 1);
	rear->audio->set_mapping(mapping);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_71_test_dcdm");
	export_dcdm(a, out);
	BOOST_REQUIRE(dcp::filesystem::exists(out / "audio" / "Lrs.wav"));

	auto b = reimport("dcdm_round_trip_71_test_b", out);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_round_trip_reels_test)
{
	auto a = new_test_film("dcdm_round_trip_reels_test_a");
	a->set_sequence(false);
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_reels_test_src_a", 24), DCPTime());
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_reels_test_src_b", 30), DCPTime::from_frames(24, 24));
	add(a, "test/data/L.wav", DCPTime::from_frames(12, 24));
	a->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(a->reels().size(), 2U);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_reels_test_dcdm");
	export_dcdm(a, out);

	auto b = reimport("dcdm_round_trip_reels_test_b", out);
	BOOST_REQUIRE_EQUAL(b->reels().size(), 2U);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_round_trip_atmos_subtitles_test)
{
	auto a = new_test_film("dcdm_round_trip_atmos_subtitles_test_a");
	a->set_sequence(false);
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_atmos_subtitles_test_src", 24), DCPTime());
	add(a, "test/data/atmos_0.mxf", DCPTime());
	auto subs = add(a, "test/data/dcp_sub3.xml", DCPTime());
	subs->set_trim_end(ContentTime::from_seconds(subs->full_length(a).seconds() - 1));
	subs->only_text()->set_use(true);
	subs->only_text()->set_burn(false);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_atmos_subtitles_test_dcdm");
	export_dcdm(a, out);

	auto b = reimport("dcdm_round_trip_atmos_subtitles_test_b", out);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_round_trip_j2k_test)
{
	auto a = new_test_film("dcdm_round_trip_j2k_test_a");
	a->set_sequence(false);
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_j2k_test_src_a", 24), DCPTime());
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_j2k_test_src_b", 30), DCPTime::from_frames(24, 24));
	auto sound = add(a, "test/data/L.wav", DCPTime());
	sound->audio->set_gain(-3);
	a->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(a->reels().size(), 2U);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_j2k_test_dcdm");
	export_dcdm(a, out, DCDMPictureFormat::J2K_LOSSLESS);
	BOOST_REQUIRE(dcp::filesystem::exists(out / "reel_01" / "picture" / "00000000.pdc.j2c"));

	auto b = reimport("dcdm_round_trip_j2k_test_b", out);
	BOOST_REQUIRE_EQUAL(b->reels().size(), 2U);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	dcp::filesystem::remove_all(out);
}


/** HI, VI and a closed caption track come back as they were, and so does the track's name, from DCDM_INFO.txt
 *  or, for a DCDM written before it gave the names, from the file name.
 */
BOOST_AUTO_TEST_CASE(dcdm_round_trip_hi_vi_captions_test)
{
	auto a = new_test_film("dcdm_round_trip_hi_vi_captions_test_a");
	a->set_sequence(false);
	a->set_audio_channels(8);
	add(a, make_dpx_sequence("build/test/dcdm_round_trip_hi_vi_captions_test_src", 48), DCPTime());
	for (auto channel: { dcp::Channel::LEFT, dcp::Channel::HI, dcp::Channel::VI }) {
		auto sound = add(a, "test/data/L.wav", DCPTime());
		auto mapping = sound->audio->mapping();
		mapping.make_zero();
		mapping.set(0, channel, 1);
		sound->audio->set_mapping(mapping);
	}
	auto caption = add(a, "test/data/dcp_sub3.xml", DCPTime());
	caption->set_trim_end(ContentTime::from_seconds(caption->full_length(a).seconds() - 2));
	caption->only_text()->set_type(TextType::CLOSED_CAPTION);
	caption->only_text()->set_dcp_track(DCPTextTrack("Catalan CCAP", dcp::LanguageTag("ca")));
	caption->only_text()->set_use(true);
	make_and_verify_dcp(a, ignore());

	auto const out = boost::filesystem::path("build/test/dcdm_round_trip_hi_vi_captions_test_dcdm");
	export_dcdm(a, out);
	BOOST_REQUIRE(dcp::filesystem::exists(out / "audio" / "HI.wav"));
	BOOST_REQUIRE(dcp::filesystem::exists(out / "audio" / "VI.wav"));
	BOOST_REQUIRE(dcp::filesystem::exists(out / "captions" / "01_Catalan_CCAP.xml"));

	auto b = reimport("dcdm_round_trip_hi_vi_captions_test_b", out);
	BOOST_CHECK_EQUAL(b->closed_text_tracks().size(), 1U);
	make_and_verify_dcp(b, ignore());
	check_same_dcp(a, b);

	/* Without the track names in DCDM_INFO.txt the name comes from the file name, with spaces for _ */
	auto info = dcp::file_to_string(out / "DCDM_INFO.txt");
	auto const track = info.find("  Track: ");
	BOOST_REQUIRE(track != string::npos);
	info.erase(track, info.find('\n', track) + 1 - track);
	dcp::File(out / "DCDM_INFO.txt", "w").checked_write(info.c_str(), info.size());
	auto c = reimport("dcdm_round_trip_hi_vi_captions_test_c", out);
	make_and_verify_dcp(c, ignore());
	check_same_dcp(a, c);

	dcp::filesystem::remove_all(out);
}


/** Each part of each reel is placed at the start of its reel and set up to make the DCP */
BOOST_AUTO_TEST_CASE(dcdm_import_set_up_test)
{
	auto a = new_test_film("dcdm_import_set_up_test_a");
	a->set_sequence(false);
	add(a, make_dpx_sequence("build/test/dcdm_import_set_up_test_src_a", 24), DCPTime());
	add(a, make_dpx_sequence("build/test/dcdm_import_set_up_test_src_b", 30), DCPTime::from_frames(24, 24));
	add(a, "test/data/L.wav", DCPTime());
	add(a, "test/data/atmos_0.mxf", DCPTime());
	a->set_reel_type(ReelType::BY_VIDEO_CONTENT);

	auto const out = boost::filesystem::path("build/test/dcdm_import_set_up_test_dcdm");
	export_dcdm(a, out, DCDMPictureFormat::J2K_LOSSLESS);

	auto b = reimport("dcdm_import_set_up_test_b", out);
	BOOST_CHECK(b->reencode_j2k());
	BOOST_CHECK(b->reel_type() == ReelType::BY_VIDEO_CONTENT);
	BOOST_CHECK(!b->sequence());
	BOOST_CHECK_EQUAL(b->video_frame_rate(), 24);
	auto const reels = a->reels();
	BOOST_REQUIRE(reels.size() > 1);
	BOOST_REQUIRE_EQUAL(b->reels().size(), reels.size());

	int pictures = 0;
	for (auto content: b->content()) {
		/* Everything in reel_NN starts where reel NN of the original film started */
		auto const path = content->path(0).string();
		auto const folder = path.find("/reel_");
		BOOST_REQUIRE(folder != string::npos);
		auto const reel = std::stoi(path.substr(folder + 6, 2)) - 1;
		BOOST_CHECK_MESSAGE(content->position() == reels[reel].from, path << " is at " << content->position().get());
		if (content->video) {
			++pictures;
			BOOST_CHECK(!content->video->colour_conversion());
		}
		if (content->audio) {
			/* Each file goes to the channel that it is named after */
			auto const mapping = content->audio->mapping();
			int mapped = 0;
			for (int output = 0; output < mapping.output_channels(); ++output) {
				if (mapping.get(0, output) != 0) {
					++mapped;
				}
			}
			BOOST_CHECK_EQUAL(mapped, 1);
			auto const name = content->path(0).filename().string();
			if (name == "L.wav") {
				BOOST_CHECK_EQUAL(mapping.get(0, dcp::Channel::LEFT), 1);
			} else if (name == "Rs.wav") {
				BOOST_CHECK_EQUAL(mapping.get(0, dcp::Channel::RS), 1);
			}
		}
	}
	BOOST_CHECK_EQUAL(pictures, static_cast<int>(reels.size()));

	dcp::filesystem::remove_all(out);
}


/** A 3D DCDM gives left and right eye content at the start of each reel */
BOOST_AUTO_TEST_CASE(dcdm_import_3d_test)
{
	auto a = new_test_film("dcdm_import_3d_test_a");
	auto source = add(a, make_dpx_sequence("build/test/dcdm_import_3d_test_src", 12), DCPTime());
	source->video->set_frame_type(VideoFrameType::THREE_D_ALTERNATE);
	a->set_three_d(true);

	auto const out = boost::filesystem::path("build/test/dcdm_import_3d_test_dcdm");
	export_dcdm(a, out);

	auto b = reimport("dcdm_import_3d_test_b", out);
	BOOST_CHECK(b->three_d());
	int left = 0;
	int right = 0;
	for (auto content: b->content()) {
		if (content->video) {
			BOOST_CHECK(content->position() == DCPTime());
			left += content->video->frame_type() == VideoFrameType::THREE_D_LEFT ? 1 : 0;
			right += content->video->frame_type() == VideoFrameType::THREE_D_RIGHT ? 1 : 0;
		}
	}
	BOOST_CHECK_EQUAL(left, 1);
	BOOST_CHECK_EQUAL(right, 1);
	make_and_verify_dcp(b, ignore());

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_import_valid_test)
{
	auto const dir = boost::filesystem::path("build/test/dcdm_import_valid_test");
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	BOOST_CHECK(!DCDMImport::valid(dir));
	BOOST_CHECK_THROW(DCDMImport{dir}, std::runtime_error);

	dcp::File(dir / "DCDM_INFO.txt", "w").checked_write("Frame rate: 25 fps\n", 19);
	BOOST_CHECK(DCDMImport::valid(dir));
	BOOST_CHECK_EQUAL(DCDMImport(dir).frame_rate().get_value_or(0), 25);
	BOOST_CHECK_EQUAL(DCDMImport(dir).reels(), 1);
}
