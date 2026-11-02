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


/** @file  test/sound_mxf_test.cc
 *  @brief Tests of SoundMXFContent, a PCM sound MXF added to a film on its own.
 *  @ingroup feature
 */


#include "lib/audio_analysis.h"
#include "lib/audio_buffers.h"
#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/examine_content_job.h"
#include "lib/film.h"
#include "lib/frame_rate_change.h"
#include "lib/hints.h"
#include "lib/job_manager.h"
#include "lib/make_dcp.h"
#include "lib/signal_manager.h"
#include "lib/player.h"
#include "lib/sound_mxf_content.h"
#include "lib/transcode_job.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/language_tag.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>


using std::dynamic_pointer_cast;
using std::string;


/** Make a DCP with a sound track and @return the path of its PCM sound MXF */
static boost::filesystem::path
make_sound_mxf(string name)
{
	auto film = new_test_film(name, { content_factory("test/data/sine_16_48_220_10.wav")[0] });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return find_file(film->dir(film->dcp_name()), "pcm_");
}


/** A PCM sound MXF is recognised as sound MXF content, not as something for FFmpeg */
BOOST_AUTO_TEST_CASE(sound_mxf_is_recognised_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_is_recognised_test_source");
	BOOST_CHECK(SoundMXFContent::valid_mxf(mxf));

	auto content = content_factory(mxf);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_CHECK(dynamic_pointer_cast<SoundMXFContent>(content[0]));

	auto film = new_test_film("sound_mxf_is_recognised_test", content);
	BOOST_REQUIRE(content[0]->audio);
	BOOST_CHECK(!content[0]->video);

	dcp::SoundAsset const asset(mxf);
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->channels(), asset.channels());
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->frame_rate(), asset.sampling_rate());
}


/** The sound of a DCP made from a sound MXF has the length of the file */
BOOST_AUTO_TEST_CASE(sound_mxf_makes_a_dcp_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_makes_a_dcp_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_makes_a_dcp_test", content_factory(mxf));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->actual_duration(), source.intrinsic_duration());
}


/** Write a 2 second sound MXF at 24fps and 48kHz with a 440Hz tone in each channel that is not in silent
 *  @param labels true to write the MCA channel labels (libdcp labels the 5.1 channels and any extra active ones)
 *  @param silent the channels that have no sound
 */
static boost::filesystem::path
write_sound_mxf(
	string name, int channels, bool labels, std::vector<int> silent, int sampling_rate = 48000, int edit_rate = 24, boost::optional<dcp::Key> key = {}
	)
{
	auto const path = boost::filesystem::path("build") / "test" / (name + ".mxf");
	boost::filesystem::remove(path);

	dcp::SoundAsset asset(dcp::Fraction(edit_rate, 1), sampling_rate, channels, dcp::LanguageTag("en-US"), dcp::Standard::SMPTE);
	if (key) {
		asset.set_key(*key);
	}
	auto writer = asset.start_write(
		path, {}, dcp::SoundAsset::AtmosSync::DISABLED,
		labels ? dcp::SoundAsset::MCASubDescriptors::ENABLED : dcp::SoundAsset::MCASubDescriptors::DISABLED
		);

	int const block = sampling_rate / edit_rate;
	std::vector<float> tone(block);
	for (int i = 0; i < block; ++i) {
		tone[i] = 0.5 * sin(2 * M_PI * 440 * i / static_cast<double>(sampling_rate));
	}
	std::vector<float> quiet(block, 0.0F);

	std::vector<float const*> data(channels);
	for (int c = 0; c < channels; ++c) {
		data[c] = std::find(silent.begin(), silent.end(), c) == silent.end() ? tone.data() : quiet.data();
	}

	for (int i = 0; i < 2 * edit_rate; ++i) {
		writer->write(data.data(), channels, block);
	}
	writer->finalize();

	return path;
}


/** @return the sound MXF content of a new film with a file added to it */
static std::shared_ptr<SoundMXFContent>
add_sound_mxf(std::shared_ptr<Film> film, boost::filesystem::path path)
{
	auto content = content_factory(path);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	auto sound = dynamic_pointer_cast<SoundMXFContent>(content[0]);
	BOOST_REQUIRE(sound);
	return sound;
}


/** @return true if input channel @param input is mapped to some output channel */
static bool
is_mapped(std::shared_ptr<SoundMXFContent> content, int input)
{
	auto const mapping = content->audio->stream()->mapping();
	for (int output = 0; output < mapping.output_channels(); ++output) {
		if (mapping.get(input, output) > 0) {
			return true;
		}
	}
	return false;
}


/** A labelled file: the channels that carry a label have sound; those without a label and with silence do not */
BOOST_AUTO_TEST_CASE(sound_mxf_labelled_channels_test)
{
	auto film = new_test_film("sound_mxf_labelled_channels_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_labelled_channels_test_source", 8, true, { 6, 7 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, true, true, true, false, false }));
	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(is_mapped(sound, i), "channel " << i);
	}
	BOOST_CHECK(!is_mapped(sound, 6));
	BOOST_CHECK(!is_mapped(sound, 7));
	BOOST_CHECK_EQUAL(sound->audio->stream()->mapping().get(2, 2), 1.0F);
}


/** A file without labels: the channels with silence are not mapped */
BOOST_AUTO_TEST_CASE(sound_mxf_unlabelled_silence_test)
{
	auto film = new_test_film("sound_mxf_unlabelled_silence_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_unlabelled_silence_test_source", 8, false, { 3 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, false, true, true, true, true }));
	BOOST_CHECK(!is_mapped(sound, 3));
	BOOST_CHECK(is_mapped(sound, 4));
}


/** A channel with no label but with sound counts as having sound, so it is mapped */
BOOST_AUTO_TEST_CASE(sound_mxf_unlabelled_sound_test)
{
	auto film = new_test_film("sound_mxf_unlabelled_sound_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_unlabelled_sound_test_source", 8, true, { 6 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, true, true, true, false, true }));
	BOOST_CHECK(is_mapped(sound, 7));
	BOOST_CHECK(!is_mapped(sound, 6));
}


/** A file where every channel is silent is mapped one to one, so a wrong guess does not lose a channel */
BOOST_AUTO_TEST_CASE(sound_mxf_all_silent_test)
{
	auto film = new_test_film("sound_mxf_all_silent_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_all_silent_test_source", 6, false, { 0, 1, 2, 3, 4, 5 }));

	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(is_mapped(sound, i), "channel " << i);
	}
}


/** The channel information comes back when the film is saved and read again */
BOOST_AUTO_TEST_CASE(sound_mxf_xml_roundtrip_test)
{
	auto film = new_test_film("sound_mxf_xml_roundtrip_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_xml_roundtrip_test_source", 8, false, { 3 }));
	film->write_metadata();

	auto film2 = std::make_shared<Film>(film->directory().get());
	film2->read_metadata();
	BOOST_REQUIRE_EQUAL(film2->content().size(), 1U);
	auto sound2 = dynamic_pointer_cast<SoundMXFContent>(film2->content()[0]);
	BOOST_REQUIRE(sound2);
	BOOST_CHECK(sound2->channels_with_sound() == sound->channels_with_sound());
	BOOST_CHECK(!is_mapped(sound2, 3));
	BOOST_CHECK(is_mapped(sound2, 0));
}


/** Make the DCP of a film and @return the sound asset of its first (and only) reel */
static std::shared_ptr<dcp::ReelSoundAsset>
make_and_find_sound(std::shared_ptr<Film> film)
{
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_REQUIRE(reels[0]->main_sound());
	return reels[0]->main_sound();
}


/** @return the sound asset that a reel's sound refers to (the DCP has been read, so it is resolved) */
static std::shared_ptr<dcp::SoundAsset>
sound_asset_of(std::shared_ptr<dcp::ReelSoundAsset> reel_asset)
{
	auto asset = dynamic_pointer_cast<dcp::SoundAsset>(reel_asset->asset_ref().asset());
	BOOST_REQUIRE(asset);
	return asset;
}


/** @return the 24 bit samples (as integers) of one channel of one frame of a sound asset */
static std::vector<int>
samples_of(std::shared_ptr<dcp::SoundAsset> asset, int64_t frame, int channel)
{
	auto reader = asset->start_read();
	reader->set_check_hmac(false);
	auto sound_frame = reader->get_frame(frame);
	auto from = sound_frame->data();
	int const channels = asset->channels();
	int const samples = sound_frame->size() / (3 * channels);
	std::vector<int> result;
	for (int i = 0; i < samples; ++i) {
		auto const p = from + (i * channels + channel) * 3;
		result.push_back((static_cast<int>((p[0] << 8) | (p[1] << 16) | (p[2] << 24))) >> 8);
	}
	return result;
}


static int
peak_of(std::vector<int> const& samples)
{
	int peak = 0;
	for (auto s: samples) {
		peak = std::max(peak, std::abs(s));
	}
	return peak;
}


/** The file is used as it is when nothing changes it: the CPL points at its ID */
BOOST_AUTO_TEST_CASE(sound_mxf_is_reused_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_is_reused_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_is_reused_test", content);
	string why_not;
	BOOST_CHECK_MESSAGE(dynamic_pointer_cast<SoundMXFContent>(content[0])->can_reference_audio(film, why_not), why_not);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->asset_ref().id(), source.id());
}


/** A trim at the start and the end changes the entry point and duration in the CPL, not the file */
BOOST_AUTO_TEST_CASE(sound_mxf_trim_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_trim_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_trim_test", content);
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));
	content[0]->set_trim_end(dcpomatic::ContentTime::from_seconds(2));
	auto sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(sound->actual_duration(), source.intrinsic_duration() - 3 * 24);
}


/** A gain means that the sound is not the file's, so it is written again with a new ID */
BOOST_AUTO_TEST_CASE(sound_mxf_gain_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_gain_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_gain_is_rewritten_test", content);
	content[0]->audio->set_gain(3);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
}


/** A mapping that is not one to one means that the sound is not the file's, so it is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_mapping_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_mapping_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_mapping_is_rewritten_test", content);
	auto stream = content[0]->audio->stream();
	auto mapping = stream->mapping();
	mapping.set(0, 0, 0);
	mapping.set(0, 1, 1);
	stream->set_mapping(mapping);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
}


/** A channel with sound that the user does not map means that the sound is not the file's: written again */
BOOST_AUTO_TEST_CASE(sound_mxf_unmapped_sound_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_unmapped_sound_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_unmapped_sound_is_rewritten_test", content);
	auto stream = content[0]->audio->stream();
	auto mapping = stream->mapping();
	mapping.set(0, 0, 0);
	stream->set_mapping(mapping);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
}


/** A film with more channels than the file: written again */
BOOST_AUTO_TEST_CASE(sound_mxf_channels_differ_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_channels_differ_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_channels_differ_test");
	film->set_audio_channels(8);
	film->examine_and_add_content(content_factory(mxf));
	BOOST_REQUIRE(!wait_for_jobs());
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), source.intrinsic_duration());
	BOOST_CHECK_EQUAL(sound_asset_of(sound)->channels(), 8);
}


/** A file at 96kHz in a 48kHz film is resampled, so written again, with the file's duration */
BOOST_AUTO_TEST_CASE(sound_mxf_sample_rate_differs_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_sample_rate_differs_test_source", 6, true, {}, 96000);
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_sample_rate_differs_test", content_factory(mxf));
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
	BOOST_CHECK_EQUAL(sound->actual_duration(), source.intrinsic_duration());
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), source.intrinsic_duration());
	BOOST_CHECK_EQUAL(sound_asset_of(sound)->sampling_rate(), 48000);
}


/** Atmos in the film means the sound has the Atmos sync signal, so the file cannot be used: written again */
BOOST_AUTO_TEST_CASE(sound_mxf_atmos_is_rewritten_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_atmos_is_rewritten_test_source", 16, true, {});
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_atmos_is_rewritten_test");
	film->set_audio_channels(16);
	auto content = content_factory(mxf);
	content.push_back(content_factory("test/data/atmos_0.mxf")[0]);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
}


/** Other sound over the same time means the file is not the film's sound: written again */
BOOST_AUTO_TEST_CASE(sound_mxf_overlap_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_overlap_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	content.push_back(content_factory("test/data/sine_16_48_220_10.wav")[0]);
	auto film = new_test_film("sound_mxf_overlap_is_rewritten_test", content);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
}


/** Two sound MXFs one after the other in a one-reel film: neither is the whole reel, so the sound is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_two_in_a_row_test)
{
	auto const first = make_sound_mxf("sound_mxf_two_in_a_row_test_source_1");
	auto const second = make_sound_mxf("sound_mxf_two_in_a_row_test_source_2");
	dcp::SoundAsset const source_1(first);
	dcp::SoundAsset const source_2(second);

	auto content = content_factory(first);
	auto film = new_test_film("sound_mxf_two_in_a_row_test", content);
	auto content_2 = content_factory(second);
	film->examine_and_add_content(content_2);
	BOOST_REQUIRE(!wait_for_jobs());
	content_2[0]->set_position(film, content[0]->end(film));
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source_1.id());
	BOOST_CHECK(sound->asset_ref().id() != source_2.id());
	BOOST_CHECK_EQUAL(sound->actual_duration(), source_1.intrinsic_duration() + source_2.intrinsic_duration());
}


/** Making the DCP twice: the file that is in the DCP folder from the first time is replaced, with no error */
BOOST_AUTO_TEST_CASE(sound_mxf_remake_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_remake_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_remake_test", content_factory(mxf));
	BOOST_CHECK_EQUAL(make_and_find_sound(film)->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(make_and_find_sound(film)->asset_ref().id(), source.id());
}


/** A film that is saved and read again still uses the file as it is */
BOOST_AUTO_TEST_CASE(sound_mxf_reopened_film_is_reused_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_reopened_film_is_reused_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_reopened_film_is_reused_test", content_factory(mxf));
	film->write_metadata();

	auto film2 = std::make_shared<Film>(film->directory().get());
	film2->read_metadata();
	BOOST_CHECK_EQUAL(make_and_find_sound(film2)->asset_ref().id(), source.id());
}


/** A delay or a gain is applied within the length of the file: the sound written again has the file's length */
BOOST_AUTO_TEST_CASE(sound_mxf_delay_keeps_length_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_delay_keeps_length_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_delay_keeps_length_test", content);
	content[0]->audio->set_delay(500);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
	BOOST_CHECK_EQUAL(sound->actual_duration(), source.intrinsic_duration());
}


/** A gain of -6dB halves the samples of the whole file */
BOOST_AUTO_TEST_CASE(sound_mxf_gain_value_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_gain_value_test_source", 6, true, {});
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_gain_value_test", content);
	content[0]->audio->set_gain(-6);
	auto sound = make_and_find_sound(film);

	auto const expected = static_cast<double>(peak_of(samples_of(std::make_shared<dcp::SoundAsset>(mxf), 10, 2))) * pow(10, -6.0 / 20);
	auto const got = peak_of(samples_of(sound_asset_of(sound), 10, 2));
	BOOST_CHECK_MESSAGE(std::abs(got - expected) < 64, "expected " << expected << " got " << got);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), source.intrinsic_duration());
}


/** A delay moves the sound later inside the same length: the start is silent */
BOOST_AUTO_TEST_CASE(sound_mxf_delay_moves_sound_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_delay_moves_sound_test_source", 6, true, {});
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_delay_moves_sound_test", content);
	content[0]->audio->set_delay(500);
	auto sound = make_and_find_sound(film);
	auto asset = sound_asset_of(sound);

	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), source.intrinsic_duration());
	for (int frame = 0; frame < 12; ++frame) {
		BOOST_CHECK_MESSAGE(peak_of(samples_of(asset, frame, 0)) < 64, "frame " << frame);
	}
	BOOST_CHECK(peak_of(samples_of(asset, 14, 0)) > 1000000);
}


/** A speed change (the picture playing with the file is at another rate than the film) means the sound is
 *  not the file's: written again.
 */
BOOST_AUTO_TEST_CASE(sound_mxf_speed_change_is_rewritten_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_speed_change_is_rewritten_test_source", 6, true, {}, 48000, 25);
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	content.push_back(content_factory("test/data/red_30.mp4")[0]);
	auto film = new_test_film("sound_mxf_speed_change_is_rewritten_test", content);
	film->set_video_frame_rate(25);
	auto sound = dynamic_pointer_cast<SoundMXFContent>(content[0]);
	BOOST_REQUIRE(FrameRateChange(film, sound).change_speed());

	string why_not;
	BOOST_CHECK(!sound->can_reference_audio(film, why_not));
	BOOST_CHECK_MESSAGE(why_not.find("speed") != string::npos, why_not);
}


/** An SMPTE file in an Interop film cannot be used as it is */
BOOST_AUTO_TEST_CASE(sound_mxf_interop_film_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_interop_film_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_interop_film_is_rewritten_test", content);
	film->set_interop(true);

	string why_not;
	BOOST_CHECK(!dynamic_pointer_cast<SoundMXFContent>(content[0])->can_reference_audio(film, why_not));
	BOOST_CHECK_MESSAGE(why_not.find("Interop") != string::npos, why_not);
}


/** An encrypted source file is refused when it is examined, with a clear error */
BOOST_AUTO_TEST_CASE(sound_mxf_encrypted_source_is_refused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_encrypted_source_is_refused_test_source", 6, true, {}, 48000, 24, dcp::Key());

	auto film = new_test_film("sound_mxf_encrypted_source_is_refused_test");
	film->examine_and_add_content(content_factory(mxf));
	BOOST_CHECK(wait_for_jobs());
}


/** Two different files with the same name, each a whole reel, both go into the DCP as they are */
BOOST_AUTO_TEST_CASE(sound_mxf_same_name_sources_test)
{
	auto const first = make_sound_mxf("sound_mxf_same_name_sources_test_source_1");
	auto const second = make_sound_mxf("sound_mxf_same_name_sources_test_source_2");
	auto const dir_a = boost::filesystem::path("build") / "test" / "sound_mxf_same_name_sources_test_a";
	auto const dir_b = boost::filesystem::path("build") / "test" / "sound_mxf_same_name_sources_test_b";
	boost::filesystem::remove_all(dir_a);
	boost::filesystem::remove_all(dir_b);
	boost::filesystem::create_directories(dir_a);
	boost::filesystem::create_directories(dir_b);
	boost::filesystem::copy_file(first, dir_a / "sound.mxf");
	boost::filesystem::copy_file(second, dir_b / "sound.mxf");
	dcp::SoundAsset const source_a(dir_a / "sound.mxf");
	dcp::SoundAsset const source_b(dir_b / "sound.mxf");
	BOOST_REQUIRE(source_a.id() != source_b.id());

	auto content_a = content_factory(dir_a / "sound.mxf");
	auto film = new_test_film("sound_mxf_same_name_sources_test", content_a);
	auto content_b = content_factory(dir_b / "sound.mxf");
	film->examine_and_add_content(content_b);
	BOOST_REQUIRE(!wait_for_jobs());
	content_b[0]->set_position(film, content_a[0]->end(film));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ content_a[0]->end(film) });
	string why_not;
	BOOST_CHECK_MESSAGE(dynamic_pointer_cast<SoundMXFContent>(content_a[0])->can_reference_audio(film, why_not), "a: " << why_not);
	BOOST_CHECK_MESSAGE(dynamic_pointer_cast<SoundMXFContent>(content_b[0])->can_reference_audio(film, why_not), "b: " << why_not);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_REQUIRE(reels[1]->main_sound());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->asset_ref().id(), source_a.id());
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->asset_ref().id(), source_b.id());
	BOOST_CHECK_EQUAL(sound_asset_of(reels[0]->main_sound())->id(), source_a.id());
	BOOST_CHECK_EQUAL(sound_asset_of(reels[1]->main_sound())->id(), source_b.id());
}


/** A source that is the file already in the DCP folder (from an earlier DCP of this film) is kept, not deleted */
BOOST_AUTO_TEST_CASE(sound_mxf_source_in_dcp_folder_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_source_in_dcp_folder_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_source_in_dcp_folder_test", content_factory(mxf));
	BOOST_CHECK_EQUAL(make_and_find_sound(film)->asset_ref().id(), source.id());

	auto const in_dcp = find_file(film->dir(film->dcp_name()), "pcm_");
	film->remove_content(film->content());
	auto content = content_factory(in_dcp);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	string why_not;
	BOOST_CHECK_MESSAGE(dynamic_pointer_cast<SoundMXFContent>(content[0])->can_reference_audio(film, why_not), why_not);
	BOOST_CHECK_EQUAL(make_and_find_sound(film)->asset_ref().id(), source.id());
	BOOST_CHECK(boost::filesystem::exists(in_dcp));
}


/** A Player decodes the sound of a reusable file unless it is told that it feeds the DCP writer, so that
 *  the hints and the audio analysis see the sound.
 */
BOOST_AUTO_TEST_CASE(sound_mxf_player_decodes_unless_making_dcp_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_player_decodes_unless_making_dcp_test_source");
	auto film = new_test_film("sound_mxf_player_decodes_unless_making_dcp_test", content_factory(mxf));

	auto frames_from = [film](bool making_dcp) {
		Player player(film, Image::Alignment::COMPACT, false);
		if (making_dcp) {
			player.set_making_dcp();
		}
		Frame frames = 0;
		player.Audio.connect([&frames](std::shared_ptr<AudioBuffers> audio, dcpomatic::DCPTime, int) { frames += audio->frames(); });
		while (!player.pass()) {}
		return frames;
	};

	BOOST_CHECK(frames_from(false) > 0);
	BOOST_CHECK_EQUAL(frames_from(true), 0);
}


/** @return a new film set up so that the sound MXF at @param path can be used in it as it is */
static std::shared_ptr<Film>
film_for_public_mxf(string name, boost::filesystem::path path)
{
	dcp::SoundAsset const source(path);
	auto film = new_test_film(name);
	film->set_audio_channels(source.channels());
	film->set_video_frame_rate(source.edit_rate().numerator);
	film->set_interop(source.standard() == dcp::Standard::INTEROP);
	film->examine_and_add_content(content_factory(path));
	BOOST_REQUIRE(!wait_for_jobs());
	return film;
}


/** Real sound MXFs from the public test data, SMPTE and Interop, go into a DCP as they are */
BOOST_AUTO_TEST_CASE(sound_mxf_public_files_are_reused_test)
{
	int n = 0;
	for (string file: {
		"test/data/scope_dcp/pcm_6278cd90-54f6-4426-b17e-8dc491cf1d14.mxf",
		"test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf",
		"test/data/repeat_frame_test/pcm_b6c2832f-b53d-4d33-8521-30ba43f81809.mxf",
		"test/data/Iopsubs_FTR-1_F_XX-XX_MOS_2K_20220710_IOP_OV/pcm_1_1_short.srt_7d643095-1d23-4ea9-b3f2-25f2c233629f.mxf"
	}) {
		dcp::SoundAsset const source(file);
		auto film = film_for_public_mxf(fmt::format("sound_mxf_public_files_are_reused_test{}", n++), file);
		BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
		auto sound_mxf = dynamic_pointer_cast<SoundMXFContent>(film->content()[0]);
		BOOST_REQUIRE_MESSAGE(sound_mxf, file);

		string why_not;
		BOOST_CHECK_MESSAGE(sound_mxf->can_reference_audio(film, why_not), file << ": " << why_not);

		film->write_metadata();
		make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
		BOOST_REQUIRE(!wait_for_jobs());

		dcp::DCP dcp(film->dir(film->dcp_name()));
		dcp.read();
		BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
		auto const reels = dcp.cpls()[0]->reels();
		BOOST_REQUIRE(!reels.empty());
		BOOST_REQUIRE(reels[0]->main_sound());
		BOOST_CHECK_EQUAL(reels[0]->main_sound()->asset_ref().id(), source.id());
		BOOST_CHECK_EQUAL(sound_asset_of(reels[0]->main_sound())->hash(), dcp::SoundAsset(file).hash());
	}
}


/** What the audio panel's level graph and peak read: an analysis of a sound MXF that goes into the DCP as it is */
BOOST_AUTO_TEST_CASE(sound_mxf_audio_analysis_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_audio_analysis_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	boost::signals2::connection connection;
	JobManager::instance()->analyse_audio(film, film->playlist(), false, connection, [](Job::Result) {});
	BOOST_REQUIRE(!wait_for_jobs());

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** The hints analyse the real sound of a sound MXF that goes into the DCP as it is, not silence */
BOOST_AUTO_TEST_CASE(sound_mxf_hints_see_the_sound_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_hints_see_the_sound_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);

	Hints hints(film);
	hints.start();
	hints.join();
	while (signal_manager->ui_idle()) {}
	hints.rethrow();

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** What the content menu's Properties shows, for a sound MXF */
BOOST_AUTO_TEST_CASE(sound_mxf_properties_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_properties_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	BOOST_CHECK(!film->content()[0]->user_properties(film).empty());
	BOOST_CHECK(!film->content()[0]->summary().empty());
	BOOST_CHECK(!film->content()[0]->technical_summary().empty());
}


/** The content menu's Re-examine leaves a sound MXF as it was, and still usable as it is */
BOOST_AUTO_TEST_CASE(sound_mxf_re_examine_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_re_examine_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	auto sound_mxf = dynamic_pointer_cast<SoundMXFContent>(film->content()[0]);
	BOOST_REQUIRE(sound_mxf);
	auto const with_sound = sound_mxf->channels_with_sound();

	JobManager::instance()->add(std::make_shared<ExamineContentJob>(film, std::vector<std::shared_ptr<Content>>{sound_mxf}, false));
	BOOST_REQUIRE(!wait_for_jobs());

	BOOST_CHECK(sound_mxf->channels_with_sound() == with_sound);
	string why_not;
	BOOST_CHECK_MESSAGE(sound_mxf->can_reference_audio(film, why_not), why_not);
}


/** The Calculate... button sets a gain, and the gain is what the sound gets */
BOOST_AUTO_TEST_CASE(sound_mxf_gain_control_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_gain_control_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	auto sound_mxf = dynamic_pointer_cast<SoundMXFContent>(film->content()[0]);
	BOOST_REQUIRE(sound_mxf);

	string why_not;
	BOOST_CHECK(sound_mxf->can_reference_audio(film, why_not));
	sound_mxf->audio->set_gain(1);
	BOOST_CHECK(!sound_mxf->can_reference_audio(film, why_not));
	sound_mxf->audio->set_gain(0);
	BOOST_CHECK(sound_mxf->can_reference_audio(film, why_not));
}


/** A trim that is not a whole number of frames cannot be an entry point: the sound is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_part_frame_trim_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_part_frame_trim_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	auto sound_mxf = dynamic_pointer_cast<SoundMXFContent>(film->content()[0]);
	BOOST_REQUIRE(sound_mxf);
	sound_mxf->set_trim_start(film, dcpomatic::ContentTime::from_seconds(0.01));

	string why_not;
	BOOST_CHECK(!sound_mxf->can_reference_audio(film, why_not));
	BOOST_CHECK_MESSAGE(why_not.find("whole frames") != string::npos, why_not);
}


/** In a film that encrypts its sound the file cannot be used: the sound is written encrypted */
BOOST_AUTO_TEST_CASE(sound_mxf_encrypted_film_is_rewritten_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_encrypted_film_is_rewritten_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_encrypted_film_is_rewritten_test", content_factory(mxf));
	film->set_encrypt_sound(true);
	/* Not verified: an encrypted DCP cannot be verified without its KDM */
	film->write_metadata();
	make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_REQUIRE(!wait_for_jobs());

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_CHECK(reels[0]->main_sound()->asset_ref().id() != source.id());
	BOOST_CHECK(reels[0]->main_sound()->key_id());
}


BOOST_AUTO_TEST_CASE(sound_mxf_gain_keeps_length_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_gain_keeps_length_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_gain_keeps_length_test", content);
	content[0]->audio->set_gain(-6);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->actual_duration(), source.intrinsic_duration());
}
