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
#include "lib/audio_processor.h"
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
#include "lib/wav_ends.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/language_tag.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>
#include <functional>


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


/** The picture's reels stay as they are: a sound MXF over two of them is one file, cut for each reel in the CPL */
BOOST_AUTO_TEST_CASE(sound_mxf_over_two_reels_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_over_two_reels_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf)[0];
	auto film = new_test_film("sound_mxf_over_two_reels_test", { content });
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(48, film->video_frame_rate()) });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->actual_duration(), 48);
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->entry_point().get_value_or(0), 48);
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->actual_duration(), source.intrinsic_duration() - 48);
}


/** Make a DCP of a WAV with @param trim seconds cut from each end, and @return its only reel's sound */
static std::shared_ptr<dcp::ReelSoundAsset>
trimmed_wav_sound(string name, double trim, std::function<void (std::shared_ptr<Film>, std::shared_ptr<Content>)> change)
{
	auto content = content_factory("test/data/sine_16_48_220_10.wav")[0];
	auto film = new_test_film(name, { content });
	content->set_trim_start(film, dcpomatic::ContentTime::from_seconds(trim));
	content->set_trim_end(dcpomatic::ContentTime::from_seconds(trim));
	change(film, content);
	return make_and_find_sound(film);
}


/** A trimmed WAV keeps its whole length in the sound asset, and the trims are in the CPL */
BOOST_AUTO_TEST_CASE(wav_keeps_whole_length_test)
{
	auto const sound = trimmed_wav_sound(
		"wav_keeps_whole_length_test", 1, [](std::shared_ptr<Film>, std::shared_ptr<Content>) {}
		);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(sound->actual_duration(), 192);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), 240);
}


/** A gain is applied to the whole of the WAV, including the parts that the trims cut off */
BOOST_AUTO_TEST_CASE(wav_gain_reaches_the_ends_test)
{
	auto const plain = trimmed_wav_sound(
		"wav_gain_reaches_the_ends_test_plain", 1, [](std::shared_ptr<Film>, std::shared_ptr<Content>) {}
		);
	auto const quiet = trimmed_wav_sound(
		"wav_gain_reaches_the_ends_test_quiet", 1,
		[](std::shared_ptr<Film>, std::shared_ptr<Content> content) { content->audio->set_gain(-6); }
		);

	auto const plain_peak = peak_of(samples_of(sound_asset_of(plain), 0, 2));
	auto const quiet_peak = peak_of(samples_of(sound_asset_of(quiet), 0, 2));
	BOOST_REQUIRE(plain_peak > 0);
	BOOST_CHECK_CLOSE(static_cast<double>(quiet_peak) / plain_peak, 0.5012, 2);
}


/** A trim that is not a whole number of frames cannot be an entry point: the WAV is cut as it always was */
BOOST_AUTO_TEST_CASE(wav_part_frame_trim_is_cut_test)
{
	auto const sound = trimmed_wav_sound(
		"wav_part_frame_trim_is_cut_test", 1.01, [](std::shared_ptr<Film>, std::shared_ptr<Content>) {}
		);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 0);
}


/** WAVs for different channels, with the same trims, keep their whole length together */
BOOST_AUTO_TEST_CASE(wavs_for_channels_keep_whole_length_test)
{
	auto left = content_factory("test/data/sine_16_48_220_10.wav")[0];
	auto right = content_factory("test/data/sine_16_48_440_10.wav")[0];
	auto film = new_test_film("wavs_for_channels_keep_whole_length_test", { left, right });
	film->set_sequence(false);
	right->set_position(film, dcpomatic::DCPTime());
	BOOST_REQUIRE(left->end(film) == right->end(film));
	for (auto content: { left, right }) {
		content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
		content->set_trim_end(dcpomatic::ContentTime::from_frames(12, 24));
	}
	auto const length = (left->full_length(film)).frames_round(24);
	auto const sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(sound->actual_duration(), length - 18);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), length);
}


/** WAVs with different trims do not line up, so they keep nothing */
BOOST_AUTO_TEST_CASE(wavs_with_different_trims_are_cut_test)
{
	auto left = content_factory("test/data/sine_16_48_220_10.wav")[0];
	auto right = content_factory("test/data/sine_16_48_440_10.wav")[0];
	auto film = new_test_film("wavs_with_different_trims_are_cut_test", { left, right });
	film->set_sequence(false);
	right->set_position(film, dcpomatic::DCPTime());
	left->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
	auto const sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 0);
}


/** A WAV with other sound over it keeps nothing, as its ends would not be the film's sound */
BOOST_AUTO_TEST_CASE(wav_with_other_sound_is_cut_test)
{
	auto content = content_factory("test/data/sine_16_48_220_10.wav")[0];
	auto other = content_factory("test/data/sine_16_48_440_10.wav")[0];
	auto film = new_test_film("wav_with_other_sound_is_cut_test", { content, other });
	film->set_sequence(false);
	content->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));
	other->set_position(film, dcpomatic::DCPTime());
	auto const sound = make_and_find_sound(film);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 0);
}


/** Make a picture MXF of 2 seconds or @param length frames, from a DCP.  @return its path */
static boost::filesystem::path
make_picture_mxf(string name, int length = 48)
{
	auto image = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film(name, { image });
	image->video->set_length(length);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return dcp_file(film, "j2c_");
}


/** Make the DCP of a film and @return the reels of its CPL */
static std::vector<std::shared_ptr<dcp::Reel>>
make_and_find_reels(std::shared_ptr<Film> film)
{
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	return dcp.cpls()[0]->reels();
}


/** A picture MXF goes into the DCP as it is */
BOOST_AUTO_TEST_CASE(picture_mxf_is_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_is_reused_test_source");
	dcp::MonoJ2KPictureAsset const source(mxf);
	auto const reels = make_and_find_reels(new_test_film("picture_mxf_is_reused_test", content_factory(mxf)));
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->asset_ref().id(), source.id());
}


/** A trim of a picture MXF is in the CPL, and the file is the same */
BOOST_AUTO_TEST_CASE(picture_mxf_trim_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_trim_test_source");
	dcp::MonoJ2KPictureAsset const source(mxf);
	auto content = content_factory(mxf)[0];
	auto film = new_test_film("picture_mxf_trim_test", { content });
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
	content->set_trim_end(dcpomatic::ContentTime::from_frames(12, 24));
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	auto const picture = reels[0]->main_picture();
	BOOST_CHECK_EQUAL(picture->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(picture->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(picture->actual_duration(), 30);
	BOOST_CHECK_EQUAL(picture->intrinsic_duration(), 48);
}


/** Two picture MXFs in reels of their own, as the film's reel type makes them, are each used as they are */
BOOST_AUTO_TEST_CASE(picture_mxf_two_in_a_row_test)
{
	auto const mxf1 = make_picture_mxf("picture_mxf_two_in_a_row_test_source1");
	auto const mxf2 = make_picture_mxf("picture_mxf_two_in_a_row_test_source2");
	dcp::MonoJ2KPictureAsset const source1(mxf1);
	dcp::MonoJ2KPictureAsset const source2(mxf2);
	auto content = content_factory(mxf1);
	content.push_back(content_factory(mxf2)[0]);
	auto film = new_test_film("picture_mxf_two_in_a_row_test", content);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->asset_ref().id(), source1.id());
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->asset_ref().id(), source2.id());
}


/** A picture MXF never adds a reel: two of them in a one-reel film stay in one reel, which is written */
BOOST_AUTO_TEST_CASE(picture_mxf_keeps_the_film_reels_test)
{
	auto const mxf1 = make_picture_mxf("picture_mxf_keeps_the_film_reels_test_source1");
	auto const mxf2 = make_picture_mxf("picture_mxf_keeps_the_film_reels_test_source2");
	dcp::MonoJ2KPictureAsset const source1(mxf1);
	auto content = content_factory(mxf1);
	content.push_back(content_factory(mxf2)[0]);
	auto const reels = make_and_find_reels(new_test_film("picture_mxf_keeps_the_film_reels_test", content));
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_CHECK(reels[0]->main_picture()->asset_ref().id() != source1.id());
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->actual_duration(), 96);
}


/** A picture MXF over two of the film's reels is one file, cut for each reel in the CPL */
BOOST_AUTO_TEST_CASE(picture_mxf_over_two_reels_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_over_two_reels_test_source");
	dcp::MonoJ2KPictureAsset const source(mxf);
	auto film = new_test_film("picture_mxf_over_two_reels_test", content_factory(mxf));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(24, film->video_frame_rate()) });
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->asset_ref().id(), source.id());
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->actual_duration(), 24);
}


/** Anything that changes the image means the file cannot be used: written again */
BOOST_AUTO_TEST_CASE(picture_mxf_changed_is_rewritten_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_changed_is_rewritten_test_source");
	dcp::MonoJ2KPictureAsset const source(mxf);

	std::vector<std::pair<string, std::function<void (std::shared_ptr<Film>, std::shared_ptr<Content>)>>> const changes = {
		{ "crop", [](std::shared_ptr<Film>, std::shared_ptr<Content> c) { c->video->set_left_crop(8); } },
		{ "fade", [](std::shared_ptr<Film>, std::shared_ptr<Content> c) { c->video->set_fade_in(4); } },
		{ "reencode", [](std::shared_ptr<Film> f, std::shared_ptr<Content>) { f->set_reencode_j2k(true); } },
		{ "subtitle", [](std::shared_ptr<Film> f, std::shared_ptr<Content>) {
			auto text = content_factory("test/data/subrip4.srt")[0];
			f->examine_and_add_content({ text });
			BOOST_REQUIRE(!wait_for_jobs());
			text->only_text()->set_use(true);
			text->only_text()->set_burn(true);
			text->set_position(f, dcpomatic::DCPTime());
		} }
	};

	for (auto const& change: changes) {
		auto content = content_factory(mxf)[0];
		auto film = new_test_film("picture_mxf_changed_is_rewritten_test_" + change.first, { content });
		change.second(film, content);
		auto const reels = make_and_find_reels(film);
		BOOST_REQUIRE(!reels.empty());
		BOOST_CHECK_MESSAGE(reels[0]->main_picture()->asset_ref().id() != source.id(), change.first);
	}
}


/** A changed picture MXF is written again at the file's whole length, and the trims stay in the CPL */
BOOST_AUTO_TEST_CASE(picture_mxf_changed_keeps_whole_length_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_changed_keeps_whole_length_test_source");
	dcp::MonoJ2KPictureAsset const source(mxf);
	auto content = content_factory(mxf)[0];
	auto film = new_test_film("picture_mxf_changed_keeps_whole_length_test", { content });
	content->video->set_left_crop(8);
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
	content->set_trim_end(dcpomatic::ContentTime::from_frames(12, 24));
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	auto const picture = reels[0]->main_picture();
	BOOST_CHECK(picture->asset_ref().id() != source.id());
	BOOST_CHECK_EQUAL(picture->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(picture->actual_duration(), 30);
	BOOST_CHECK_EQUAL(picture->intrinsic_duration(), 48);
}


/** The same with the film only re-encoding: the file's frames are written again too */
BOOST_AUTO_TEST_CASE(picture_mxf_reencoded_keeps_whole_length_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_reencoded_keeps_whole_length_test_source");
	auto content = content_factory(mxf)[0];
	auto film = new_test_film("picture_mxf_reencoded_keeps_whole_length_test", { content });
	film->set_reencode_j2k(true);
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(12, 24));
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->entry_point().get_value_or(0), 12);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->intrinsic_duration(), 48);
}


/** Over two reels of the film, the head goes with the first and the tail with the last */
BOOST_AUTO_TEST_CASE(picture_mxf_changed_over_two_reels_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_changed_over_two_reels_test_source", 96);
	auto content = content_factory(mxf)[0];
	auto film = new_test_film("picture_mxf_changed_over_two_reels_test", { content });
	content->video->set_left_crop(8);
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
	content->set_trim_end(dcpomatic::ContentTime::from_frames(6, 24));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(42, 24) });
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->actual_duration(), 42);
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->intrinsic_duration(), 48);
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->actual_duration(), 42);
	BOOST_CHECK_EQUAL(reels[1]->main_picture()->intrinsic_duration(), 48);
}


/** With other video over it, the file is cut as it always was */
BOOST_AUTO_TEST_CASE(picture_mxf_changed_with_other_video_is_cut_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_changed_with_other_video_is_cut_test_source");
	auto content = content_factory(mxf)[0];
	auto other = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("picture_mxf_changed_with_other_video_is_cut_test", { content, other });
	film->set_sequence(false);
	other->set_position(film, dcpomatic::DCPTime());
	other->video->set_length(48);
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE(!reels.empty());
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->entry_point().get_value_or(0), 0);
}


/** Make a DCP with the picture MXF @param mxf as @param count whole reels, each changed by @param change.  @return its reels */
static std::vector<std::shared_ptr<dcp::Reel>>
repeated_picture_reels(string name, boost::filesystem::path mxf, int count, std::function<void (std::shared_ptr<Film>, std::shared_ptr<Content>, int)> change)
{
	auto film = new_test_film(name);
	std::vector<dcpomatic::DCPTime> boundaries;
	std::shared_ptr<Content> previous;
	for (int i = 0; i < count; ++i) {
		auto content = content_factory(mxf);
		film->examine_and_add_content(content);
		BOOST_REQUIRE(!wait_for_jobs());
		change(film, content[0], i);
		if (previous) {
			content[0]->set_position(film, previous->end(film));
			boundaries.push_back(previous->end(film));
		}
		previous = content[0];
	}
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries(boundaries);
	auto const reels = make_and_find_reels(film);
	BOOST_REQUIRE_EQUAL(reels.size(), static_cast<size_t>(count));
	return reels;
}


/** @return the number of picture MXFs in the DCP of the test film @param name */
static int
picture_files_in(string name)
{
	int count = 0;
	for (auto const& entry: boost::filesystem::recursive_directory_iterator(boost::filesystem::path("build/test") / name)) {
		auto const filename = entry.path().filename().string();
		if (boost::starts_with(filename, "j2c_") && entry.path().extension() == ".mxf" && entry.path().parent_path().parent_path().filename() == name) {
			++count;
		}
	}
	return count;
}


/** The same file as three whole reels, cropped and trimmed the same: one asset of the whole length, used by every reel */
BOOST_AUTO_TEST_CASE(picture_mxf_repeated_changed_is_one_asset_test)
{
	string const name = "picture_mxf_repeated_changed_is_one_asset_test";
	auto const mxf = make_picture_mxf(name + "_source", 72);
	auto const reels = repeated_picture_reels(name, mxf, 3, [](std::shared_ptr<Film> film, std::shared_ptr<Content> content, int) {
		content->video->set_left_crop(8);
		content->set_trim_start(film, dcpomatic::ContentTime::from_frames(6, 24));
		content->set_trim_end(dcpomatic::ContentTime::from_frames(6, 24));
	});
	for (auto reel: reels) {
		BOOST_CHECK_EQUAL(reel->main_picture()->asset_ref().id(), reels[0]->main_picture()->asset_ref().id());
		BOOST_CHECK_EQUAL(reel->main_picture()->entry_point().get_value_or(0), 6);
		BOOST_CHECK_EQUAL(reel->main_picture()->actual_duration(), 60);
		BOOST_CHECK_EQUAL(reel->main_picture()->intrinsic_duration(), 72);
	}
	BOOST_CHECK_EQUAL(picture_files_in(name), 1);
}


/** Different changes are different assets */
BOOST_AUTO_TEST_CASE(picture_mxf_repeated_different_changes_test)
{
	string const name = "picture_mxf_repeated_different_changes_test";
	auto const mxf = make_picture_mxf(name + "_source", 48);
	auto const reels = repeated_picture_reels(name, mxf, 3, [](std::shared_ptr<Film>, std::shared_ptr<Content> content, int index) {
		content->video->set_left_crop(index == 2 ? 16 : 8);
	});
	BOOST_CHECK_EQUAL(reels[0]->main_picture()->asset_ref().id(), reels[1]->main_picture()->asset_ref().id());
	BOOST_CHECK(reels[0]->main_picture()->asset_ref().id() != reels[2]->main_picture()->asset_ref().id());
	BOOST_CHECK_EQUAL(picture_files_in(name), 2);
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


/** A gain and a trim together: the new sound is the file's whole length and the trim is the entry point and duration */
BOOST_AUTO_TEST_CASE(sound_mxf_gain_with_trim_keeps_length_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_gain_with_trim_keeps_length_test_source");
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_gain_with_trim_keeps_length_test", content);
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));
	content[0]->set_trim_end(dcpomatic::ContentTime::from_seconds(2));
	content[0]->audio->set_gain(3);
	auto sound = make_and_find_sound(film);
	BOOST_CHECK(sound->asset_ref().id() != source.id());
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(sound->actual_duration(), source.intrinsic_duration() - 3 * 24);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), source.intrinsic_duration());
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


/** A fade in after a start trim rises from silence at the trim, in the file's own frames */
BOOST_AUTO_TEST_CASE(sound_mxf_fade_with_trim_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_fade_with_trim_test_source", 6, true, {});
	dcp::SoundAsset const source(mxf);

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_fade_with_trim_test", content);
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));
	content[0]->audio->set_fade_in(dcpomatic::ContentTime::from_seconds(0.5));
	auto sound = make_and_find_sound(film);
	auto asset = sound_asset_of(sound);

	/* The file is 48 frames: the head (0-23) is kept as it is, the fade runs over frames 24-35 */
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), 48);
	auto const head = peak_of(samples_of(asset, 5, 0));
	auto const start = peak_of(samples_of(asset, 24, 0));
	auto const end = peak_of(samples_of(asset, 35, 0));
	auto const after = peak_of(samples_of(asset, 44, 0));
	BOOST_CHECK_MESSAGE(head > after * 0.8, "head " << head << " after " << after);
	BOOST_CHECK_MESSAGE(start < end / 3, "start " << start << " end " << end);
	BOOST_CHECK_MESSAGE(end > after * 0.8, "end " << end << " after " << after);
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


/** A sound MXF with a start trim at the start of a reel, followed by other sound: the reel's sound asset keeps the
 *  head of the file before the trim, and the CPL's entry point skips it.
 */
BOOST_AUTO_TEST_CASE(sound_mxf_head_kept_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_head_kept_test_source", 6, true, {});

	auto content = content_factory(mxf);
	auto film = new_test_film("sound_mxf_head_kept_test", content);
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));
	auto wav = content_factory("test/data/sine_16_48_220_10.wav");
	film->examine_and_add_content(wav);
	BOOST_REQUIRE(!wait_for_jobs());
	wav[0]->set_position(film, content[0]->end(film));

	auto sound = make_and_find_sound(film);
	auto const reel_frames = film->length().frames_round(24);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(sound->actual_duration(), reel_frames);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), reel_frames + 24);
	auto asset = sound_asset_of(sound);
	BOOST_CHECK(peak_of(samples_of(asset, 0, 0)) > 1000000);
	BOOST_CHECK(peak_of(samples_of(asset, 30, 0)) > 1000000);
}


/** A sound MXF with an end trim at the end of a reel, after other sound: the reel's sound asset keeps the tail of
 *  the file after the trim, and the CPL's duration stops before it.
 */
BOOST_AUTO_TEST_CASE(sound_mxf_tail_kept_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_tail_kept_test_source", 6, true, {});

	auto wav = content_factory("test/data/sine_16_48_220_10.wav");
	auto film = new_test_film("sound_mxf_tail_kept_test", wav);
	auto content = content_factory(mxf);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	content[0]->set_position(film, wav[0]->end(film));
	content[0]->set_trim_end(dcpomatic::ContentTime::from_seconds(1));

	auto sound = make_and_find_sound(film);
	auto const reel_frames = film->length().frames_round(24);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(sound->actual_duration(), reel_frames);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), reel_frames + 24);
	auto asset = sound_asset_of(sound);
	BOOST_CHECK(peak_of(samples_of(asset, reel_frames + 12, 0)) > 1000000);
}


/** A sound MXF in the middle of a reel keeps nothing of what its trims cut off */
BOOST_AUTO_TEST_CASE(sound_mxf_middle_keeps_nothing_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_middle_keeps_nothing_test_source", 6, true, {});

	auto first = content_factory("test/data/sine_16_48_220_10.wav");
	auto film = new_test_film("sound_mxf_middle_keeps_nothing_test", first);
	auto content = content_factory(mxf);
	auto last = content_factory("test/data/sine_16_48_220_10.wav");
	film->examine_and_add_content(content);
	film->examine_and_add_content(last);
	BOOST_REQUIRE(!wait_for_jobs());
	content[0]->set_position(film, first[0]->end(film));
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(0.5));
	content[0]->set_trim_end(dcpomatic::ContentTime::from_seconds(0.5));
	last[0]->set_position(film, content[0]->end(film));

	auto sound = make_and_find_sound(film);
	auto const reel_frames = film->length().frames_round(24);
	BOOST_CHECK_EQUAL(sound->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(sound->intrinsic_duration(), reel_frames);
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


/** @return the number of PCM sound MXFs in a DCP's folder */
static int
pcm_files_in(boost::filesystem::path dcp)
{
	int n = 0;
	for (auto i: boost::filesystem::directory_iterator(dcp)) {
		if (boost::starts_with(i.path().filename().string(), "pcm_")) {
			++n;
		}
	}
	return n;
}


/** Make a film of @param count reels, each the same sound MXF, and @return the DCP's reels after @param change
 *  has been applied to each content.
 */
static std::vector<std::shared_ptr<dcp::Reel>>
repeated_reels(string name, boost::filesystem::path mxf, int count, std::function<void (std::shared_ptr<Content>, int)> change)
{
	auto film = new_test_film(name);
	std::vector<dcpomatic::DCPTime> boundaries;
	std::shared_ptr<Content> previous;
	for (int i = 0; i < count; ++i) {
		auto content = content_factory(mxf);
		film->examine_and_add_content(content);
		BOOST_REQUIRE(!wait_for_jobs());
		if (previous) {
			content[0]->set_position(film, previous->end(film));
			boundaries.push_back(previous->end(film));
		}
		change(content[0], i);
		previous = content[0];
	}
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries(boundaries);
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), static_cast<size_t>(count));
	BOOST_CHECK_EQUAL(pcm_files_in(film->dir(film->dcp_name())), 1);
	return reels;
}


/** The same WAV as three whole reels: one sound asset, used by every reel */
BOOST_AUTO_TEST_CASE(wav_repeated_reels_same_id_test)
{
	auto const reels = repeated_reels(
		"wav_repeated_reels_same_id_test", "test/data/sine_16_48_220_10.wav", 3, [](std::shared_ptr<Content>, int) {}
		);
	BOOST_REQUIRE(reels[0]->main_sound());
	for (auto reel: reels) {
		BOOST_CHECK_EQUAL(reel->main_sound()->asset_ref().id(), reels[0]->main_sound()->asset_ref().id());
	}
}


/** The same WAV with a different gain is different sound: another asset */
BOOST_AUTO_TEST_CASE(wav_repeated_reels_different_gain_test)
{
	auto first = content_factory("test/data/sine_16_48_220_10.wav");
	auto second = content_factory("test/data/sine_16_48_220_10.wav");
	first.push_back(second[0]);
	auto film = new_test_film("wav_repeated_reels_different_gain_test", first);
	second[0]->set_position(film, first[0]->end(film));
	second[0]->audio->set_gain(-6);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ first[0]->end(film) });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	BOOST_CHECK_EQUAL(pcm_files_in(film->dir(film->dcp_name())), 2);
}


/** The same WAV with a delay is not shared, as the delay moves sound across the reels */
BOOST_AUTO_TEST_CASE(wav_repeated_reels_with_delay_are_not_shared_test)
{
	auto first = content_factory("test/data/sine_16_48_220_10.wav");
	auto second = content_factory("test/data/sine_16_48_220_10.wav");
	first.push_back(second[0]);
	auto film = new_test_film("wav_repeated_reels_with_delay_are_not_shared_test", first);
	second[0]->set_position(film, first[0]->end(film));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ first[0]->end(film) });
	BOOST_REQUIRE(wav_shared_source(film, second[0]));
	first[0]->audio->set_delay(100);
	second[0]->audio->set_delay(100);
	BOOST_CHECK(!wav_shared_source(film, second[0]));
}


/** The same file as five whole reels, unchanged: one asset, referenced by every reel */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_reels_same_id_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_repeated_reels_same_id_test_source");
	dcp::SoundAsset const source(mxf);

	auto const reels = repeated_reels("sound_mxf_repeated_reels_same_id_test", mxf, 5, [](std::shared_ptr<Content>, int) {});
	for (auto reel: reels) {
		BOOST_REQUIRE(reel->main_sound());
		BOOST_CHECK_EQUAL(reel->main_sound()->asset_ref().id(), source.id());
	}
}


/** The same file as five whole reels with the same gain: one new asset, referenced by every reel */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_reels_changed_same_id_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_repeated_reels_changed_same_id_test_source");
	dcp::SoundAsset const source(mxf);

	auto const reels = repeated_reels(
		"sound_mxf_repeated_reels_changed_same_id_test", mxf, 5, [](std::shared_ptr<Content> content, int) { content->audio->set_gain(3); }
		);
	BOOST_REQUIRE(reels[0]->main_sound());
	auto const id = reels[0]->main_sound()->asset_ref().id();
	BOOST_CHECK(id != source.id());
	for (auto reel: reels) {
		BOOST_REQUIRE(reel->main_sound());
		BOOST_CHECK_EQUAL(reel->main_sound()->asset_ref().id(), id);
	}
}


/** Two reels with the same sound MXF, gain 3 dB, for the sharing tests below */
static std::pair<std::shared_ptr<Film>, std::shared_ptr<SoundMXFContent>>
two_gained_reels(string name, std::vector<std::shared_ptr<Content>> extra = {})
{
	auto const mxf = make_sound_mxf(name + "_source");
	auto first = content_factory(mxf);
	auto second = content_factory(mxf);
	auto film = new_test_film(name, first);
	film->examine_and_add_content(second);
	if (!extra.empty()) {
		film->examine_and_add_content(extra);
	}
	BOOST_REQUIRE(!wait_for_jobs());
	film->set_sequence(false);
	second[0]->set_position(film, first[0]->end(film));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ first[0]->end(film) });
	first[0]->audio->set_gain(3);
	second[0]->audio->set_gain(3);
	auto sound = std::dynamic_pointer_cast<SoundMXFContent>(second[0]);
	BOOST_REQUIRE(sound);
	return { film, sound };
}


/** A reel with other sound over its sound MXF does not have the same sound as one without, so it is not shared */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_reels_with_other_sound_are_not_shared_test)
{
	auto music = content_factory("test/data/sine_16_48_440_10.wav")[0];
	auto const setup = two_gained_reels("sound_mxf_repeated_reels_with_other_sound_are_not_shared_test", { music });
	auto film = setup.first;
	music->set_position(film, setup.second->position());
	music->set_trim_end(dcpomatic::ContentTime(music->full_length(film).get() - setup.second->full_length(film).get()));
	BOOST_CHECK(!setup.second->shared_sound_source(film));
}


/** An audio processor has state, a delay moves sound across reels and a speed change makes sound of another length:
 *  none of these is shared
 */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_reels_that_differ_are_not_shared_test)
{
	auto const setup = two_gained_reels("sound_mxf_repeated_reels_that_differ_are_not_shared_test");
	auto film = setup.first;
	auto sound = setup.second;
	auto first = film->content().front();
	BOOST_REQUIRE(sound->shared_sound_source(film));

	film->set_audio_processor(AudioProcessor::from_id("mid-side-decoder"));
	BOOST_CHECK(!sound->shared_sound_source(film));
	film->set_audio_processor(nullptr);
	BOOST_REQUIRE(sound->shared_sound_source(film));

	first->audio->set_delay(100);
	sound->audio->set_delay(100);
	BOOST_CHECK(!sound->shared_sound_source(film));
	first->audio->set_delay(0);
	sound->audio->set_delay(0);

	sound->set_video_frame_rate(film, 25);
	BOOST_CHECK(!sound->shared_sound_source(film));
}


/** In a film with Atmos the sound is written with the Atmos sync signal, which must start with the reel: no head */
BOOST_AUTO_TEST_CASE(sound_mxf_with_atmos_keeps_no_head_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_with_atmos_keeps_no_head_test_source", 16, true, {});
	auto film = new_test_film("sound_mxf_with_atmos_keeps_no_head_test");
	film->set_audio_channels(16);
	auto content = content_factory(mxf);
	content.push_back(content_factory("test/data/atmos_0.mxf")[0]);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_frames(12, 24));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE(!reels.empty());
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->entry_point().get_value_or(0), 0);
}


/** The gain of a sound MXF goes on the frames that its trim cuts off too */
BOOST_AUTO_TEST_CASE(sound_mxf_gain_reaches_the_head_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_gain_reaches_the_head_test_source");
	auto content = content_factory(mxf)[0];
	auto film = new_test_film("sound_mxf_gain_reaches_the_head_test", { content });
	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(12, 24));
	content->audio->set_gain(-6);
	auto const sound = make_and_find_sound(film);
	BOOST_REQUIRE_EQUAL(sound->entry_point().get_value_or(0), 12);
	auto const plain = peak_of(samples_of(std::make_shared<dcp::SoundAsset>(mxf), 0, 2));
	auto const quiet = peak_of(samples_of(sound_asset_of(sound), 0, 2));
	BOOST_REQUIRE(plain > 0);
	BOOST_CHECK_CLOSE(static_cast<double>(quiet) / plain, 0.5012, 2);
}


/** The same file as two whole reels with different gains: two assets */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_reels_different_settings_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_repeated_reels_different_settings_test_source");

	auto film = new_test_film("sound_mxf_repeated_reels_different_settings_test");
	auto first = content_factory(mxf);
	auto second = content_factory(mxf);
	film->examine_and_add_content(first);
	film->examine_and_add_content(second);
	BOOST_REQUIRE(!wait_for_jobs());
	second[0]->set_position(film, first[0]->end(film));
	second[0]->audio->set_gain(3);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ first[0]->end(film) });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	BOOST_CHECK(reels[0]->main_sound()->asset_ref().id() != reels[1]->main_sound()->asset_ref().id());
	BOOST_CHECK_EQUAL(pcm_files_in(film->dir(film->dcp_name())), 2);
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
