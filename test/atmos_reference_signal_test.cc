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


#include "lib/atmos_reference_signal.h"
#include "lib/audio_buffers.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/mxf.h>
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <functional>
#include <vector>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;


/** @return an Atmos frame with Dolby's preamble (tag @p tag) carrying @p samples reference samples of @p value */
static vector<uint8_t>
frame_with_reference(int16_t value, int samples = 800, uint8_t tag = 0x0e)
{
	uint32_t const length = 3 + samples * 2;
	vector<uint8_t> frame = {
		0x01,
		static_cast<uint8_t>(length >> 24), static_cast<uint8_t>(length >> 16), static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length),
		0x11, 0x01, tag
	};
	for (int i = 0; i < samples; ++i) {
		frame.push_back(static_cast<uint8_t>(value & 0xff));
		frame.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
	}
	/* Then an IAFrame, which only has to be there */
	frame.insert(frame.end(), { 0x02, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x01, 0x00 });
	return frame;
}


/** Write an Atmos track file whose frame i carries a reference signal of value(i) */
static boost::filesystem::path
write_atmos(string name, int frames, std::function<int16_t (int)> value)
{
	auto const path = boost::filesystem::path("build/test") / (name + ".mxf");
	boost::filesystem::remove(path);
	dcp::AtmosAsset asset({24, 1}, 0, 16, 128, 0);
	asset.set_metadata(dcp::MXFMetadata("Dolby Laboratories", "test", "1"));
	auto writer = asset.start_write(path);
	for (int i = 0; i < frames; ++i) {
		auto const frame = frame_with_reference(value(i));
		writer->write(frame.data(), frame.size());
	}
	writer->finalize();
	return path;
}


/** @return all the audio the player gives for @p film, in reference-only mode or not */
static shared_ptr<AudioBuffers>
play(shared_ptr<Film> film, bool reference_only)
{
	auto all = make_shared<AudioBuffers>(film->audio_channels(), 0);
	Player player(film, Image::Alignment::COMPACT, false);
	player.set_atmos_reference_only(reference_only);
	player.Audio.connect([all](shared_ptr<AudioBuffers> audio, DCPTime, int) { all->append(audio); });
	while (!player.pass()) {}
	return all;
}


static float
level(int16_t value)
{
	return value / 32768.0f;
}


BOOST_AUTO_TEST_CASE(atmos_reference_signal_parse_test)
{
	auto check = [](vector<uint8_t> frame) {
		return atmos_reference_signal(frame.data(), frame.size());
	};

	/* Dolby's preamble tag, and the one of its older encoders */
	auto samples = check(frame_with_reference(1234));
	BOOST_REQUIRE_EQUAL(samples.size(), 800U);
	BOOST_CHECK_EQUAL(samples[0], 1234);
	BOOST_CHECK_EQUAL(samples[799], 1234);
	BOOST_CHECK_EQUAL(check(frame_with_reference(-2, 800, 0x02)).size(), 800U);
	BOOST_CHECK_EQUAL(check(frame_with_reference(-2, 800, 0x02))[400], -2);

	/* Samples are 16-bit little-endian */
	auto frame = frame_with_reference(0);
	frame[8] = 0x34;
	frame[9] = 0x12;
	frame[10] = 0xff;
	frame[11] = 0x80;
	samples = check(frame);
	BOOST_CHECK_EQUAL(samples[0], 0x1234);
	BOOST_CHECK_EQUAL(samples[1], -32513);

	/* The length gives the number of samples, here 640 as at 30fps */
	BOOST_CHECK_EQUAL(check(frame_with_reference(1, 640)).size(), 640U);

	/* Any other tag, or a frame that is not Dolby's, has none */
	for (auto i: { 0, 5, 6, 7 }) {
		auto other = frame_with_reference(1);
		other[i] ^= 0x01;
		BOOST_CHECK_MESSAGE(check(other).empty(), i);
	}
	auto tag_03 = frame_with_reference(1, 800, 0x03);
	BOOST_CHECK(check(tag_03).empty());

	/* Too short, or a length that runs past the frame */
	BOOST_CHECK(check({ 0x01, 0x00, 0x00, 0x00, 0x03, 0x11, 0x01 }).empty());
	auto long_length = frame_with_reference(1, 10);
	long_length[4] = 0xff;
	BOOST_CHECK(check(long_length).empty());
	/* A length of just the tag has no samples */
	BOOST_CHECK(check({ 0x01, 0x00, 0x00, 0x00, 0x03, 0x11, 0x01, 0x0e }).empty());
	/* A length shorter than the tag is not Dolby's preamble */
	BOOST_CHECK(check({ 0x01, 0x00, 0x00, 0x00, 0x02, 0x11, 0x01, 0x0e }).empty());
	/* A length that just fits, and one that is one byte too long for the frame */
	BOOST_CHECK_EQUAL(check({ 0x01, 0x00, 0x00, 0x00, 0x05, 0x11, 0x01, 0x0e, 0x01, 0x00 }).size(), 1U);
	auto full = frame_with_reference(7, 4);
	BOOST_CHECK(atmos_reference_signal(full.data(), 15).empty());
	BOOST_CHECK_EQUAL(atmos_reference_signal(full.data(), 16).size(), 4U);
	/* An odd byte after the tag is not a sample */
	BOOST_CHECK(check({ 0x01, 0x00, 0x00, 0x00, 0x04, 0x11, 0x01, 0x0e, 0x01, 0x00 }).empty());
	BOOST_CHECK_EQUAL(check({ 0x01, 0x00, 0x00, 0x00, 0x06, 0x11, 0x01, 0x0e, 0x01, 0x00, 0x00 }).size(), 1U);
}


BOOST_AUTO_TEST_CASE(atmos_reference_signal_audio_test)
{
	/* A ramp of 800 samples, up to 2000 at 48kHz and 24fps, in the centre of 6 channels */
	auto frame = frame_with_reference(0);
	for (int i = 0; i < 800; ++i) {
		frame[8 + i * 2] = static_cast<uint8_t>((i * 10) & 0xff);
		frame[9 + i * 2] = static_cast<uint8_t>((i * 10) >> 8);
	}
	auto audio = atmos_reference_audio(frame.data(), frame.size(), 6, 2, 2000);
	BOOST_REQUIRE(audio);
	BOOST_CHECK_EQUAL(audio->channels(), 6);
	BOOST_REQUIRE_EQUAL(audio->frames(), 2000);
	/* Sample 5 is at 2 of the reference signal; sample 6 between its 2 and 3 */
	BOOST_CHECK_CLOSE(audio->data(2)[5], level(20), 1e-3);
	BOOST_CHECK_CLOSE(audio->data(2)[6], (0.6f * 20 + 0.4f * 30) / 32768, 1e-3);
	/* The last ones hold the last sample */
	BOOST_CHECK_CLOSE(audio->data(2)[1999], level(7990), 1e-3);
	for (auto channel: { 0, 1, 3, 4, 5 }) {
		for (int i = 0; i < 2000; ++i) {
			BOOST_REQUIRE_EQUAL(audio->data(channel)[i], 0);
		}
	}

	/* Nothing for a frame without it, or a channel that does not exist */
	auto other = frame_with_reference(1);
	other[5] = 0;
	BOOST_CHECK(!atmos_reference_audio(other.data(), other.size(), 6, 2, 2000));
	BOOST_CHECK(!atmos_reference_audio(frame.data(), frame.size(), 6, 6, 2000));
	BOOST_CHECK(!atmos_reference_audio(frame.data(), frame.size(), 6, -1, 2000));
	BOOST_CHECK(!atmos_reference_audio(frame.data(), frame.size(), 6, 2, 0));
	BOOST_CHECK(atmos_reference_audio(frame.data(), frame.size(), 1, 0, 1));
}


BOOST_AUTO_TEST_CASE(atmos_reference_signal_output_mapping_test)
{
	auto const centre = static_cast<int>(dcp::Channel::CENTRE);
	auto only = [centre](AudioMapping const& mapping, vector<int> outputs) {
		for (int input = 0; input < mapping.input_channels(); ++input) {
			for (int output = 0; output < mapping.output_channels(); ++output) {
				auto const expected = input == centre && std::find(outputs.begin(), outputs.end(), output) != outputs.end() ? 1 : 0;
				if (mapping.get(input, output) != expected) {
					return false;
				}
			}
		}
		return true;
	};

	BOOST_CHECK(only(atmos_reference_output_mapping(2), { 0, 1 }));
	BOOST_CHECK(only(atmos_reference_output_mapping(6), { centre }));
	BOOST_CHECK(only(atmos_reference_output_mapping(3), { centre }));
	BOOST_CHECK(only(atmos_reference_output_mapping(1), { 0 }));
}


/** In a DCP, each Atmos frame's reference signal is heard in the centre at that frame's time, and nothing else;
 *  without reference-only the audio is the normal mix
 */
BOOST_AUTO_TEST_CASE(atmos_reference_signal_dcp_test)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto atmos = content_factory(write_atmos("atmos_reference_signal_dcp_source", 48, [](int i) { return static_cast<int16_t>((i + 1) * 500); }))[0];
	auto sound = content_factory("test/data/C.wav")[0];
	auto source = new_test_film("atmos_reference_signal_dcp_source", { picture, atmos, sound });
	picture->video->set_length(48);
	make_and_verify_dcp(source, {}, false, false);

	auto dcp = make_shared<DCPContent>(source->dir(source->dcp_name()));
	auto film = new_test_film("atmos_reference_signal_dcp", { dcp });
	BOOST_REQUIRE(dcp->atmos);
	auto const centre = static_cast<int>(dcp::Channel::CENTRE);

	auto reference = play(film, true);
	BOOST_REQUIRE_GE(reference->frames(), 96000);
	for (int i = 0; i < 48; ++i) {
		/* The middle of frame i */
		auto const sample = i * 2000 + 1000;
		BOOST_CHECK_CLOSE(reference->data(centre)[sample], level((i + 1) * 500), 1e-3);
		for (int channel = 0; channel < reference->channels(); ++channel) {
			if (channel != centre) {
				BOOST_REQUIRE_EQUAL(reference->data(channel)[sample], 0);
			}
		}
	}

	auto normal = play(film, false);
	BOOST_REQUIRE_EQUAL(normal->frames(), reference->frames());
	bool sound_heard = false;
	for (int i = 0; i < 48; ++i) {
		auto const sample = i * 2000 + 1000;
		BOOST_CHECK(std::abs(normal->data(centre)[sample] - level((i + 1) * 500)) > 1e-4);
		sound_heard = sound_heard || normal->data(centre)[sample] != 0;
	}
	BOOST_CHECK(sound_heard);
}


/** An Atmos MXF that starts two frames after the sound is heard two frames late: the reference signal shows where
 *  the Atmos really is, and none of it is lost when the sound comes earlier than the Atmos in the player
 */
BOOST_AUTO_TEST_CASE(atmos_reference_signal_offset_test)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto atmos = content_factory(write_atmos("atmos_reference_signal_offset_source", 24, [](int i) { return static_cast<int16_t>((i + 1) * 1000); }))[0];
	auto sound = content_factory("test/data/C.wav")[0];
	auto film = new_test_film("atmos_reference_signal_offset", { picture, atmos, sound });
	picture->video->set_length(48);
	atmos->set_position(film, DCPTime::from_frames(2, 24));
	auto const centre = static_cast<int>(dcp::Channel::CENTRE);

	auto reference = play(film, true);
	BOOST_REQUIRE_GE(reference->frames(), 26 * 2000);
	/* Every sample of every frame: none of the signal is lost */
	int wrong = 0;
	for (int i = 0; i < 26; ++i) {
		auto const expected = i < 2 ? 0 : level((i - 1) * 1000);
		for (int sample = i * 2000; sample < (i + 1) * 2000; ++sample) {
			if (std::abs(reference->data(centre)[sample] - expected) > 1e-4) {
				++wrong;
			}
		}
	}
	BOOST_CHECK_EQUAL(wrong, 0);
	/* After the Atmos ends, silence */
	BOOST_CHECK_EQUAL(reference->data(centre)[27 * 2000], 0);
}


/** A film with fewer channels than the centre gets the reference signal in its first channel */
BOOST_AUTO_TEST_CASE(atmos_reference_signal_stereo_film_test)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto atmos = content_factory(write_atmos("atmos_reference_signal_stereo_film", 24, [](int) { return static_cast<int16_t>(4000); }))[0];
	auto film = new_test_film("atmos_reference_signal_stereo_film", { picture, atmos });
	picture->video->set_length(24);
	film->set_audio_channels(2);

	auto reference = play(film, true);
	BOOST_REQUIRE_EQUAL(reference->channels(), 2);
	BOOST_REQUIRE_GE(reference->frames(), 48000);
	BOOST_CHECK_CLOSE(reference->data(0)[24000], level(4000), 1e-3);
	BOOST_CHECK_EQUAL(reference->data(1)[24000], 0);
}


/** The choice is named Atmos when all the film's immersive audio is Dolby's, as the ISDCF name has it, and IAB otherwise */
BOOST_AUTO_TEST_CASE(atmos_reference_signal_name_test)
{
	using dcpomatic::film::immersive_audio_is_dolby;

	auto const iab_path = boost::filesystem::path("build/test/atmos_reference_signal_name_iab.mxf");
	boost::filesystem::remove(iab_path);
	dcp::AtmosAsset asset({24, 1}, 0, 16, 128, 0);
	asset.set_metadata(dcp::MXFMetadata("Another encoder", "test", "1"));
	auto writer = asset.start_write(iab_path);
	vector<uint8_t> const frame = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x01, 0x00 };
	for (int i = 0; i < 24; ++i) {
		writer->write(frame.data(), frame.size());
	}
	writer->finalize();

	auto dolby = content_factory(write_atmos("atmos_reference_signal_name_dolby", 24, [](int) { return static_cast<int16_t>(100); }))[0];
	auto iab = content_factory(iab_path)[0];

	BOOST_CHECK(immersive_audio_is_dolby(new_test_film("atmos_reference_signal_name_dolby", { dolby })));
	BOOST_CHECK(!immersive_audio_is_dolby(new_test_film("atmos_reference_signal_name_iab", { iab })));
	auto both = new_test_film("atmos_reference_signal_name_both", { content_factory(write_atmos("atmos_reference_signal_name_both_dolby", 24, [](int) { return static_cast<int16_t>(100); }))[0], content_factory(iab_path)[0] });
	BOOST_CHECK(!immersive_audio_is_dolby(both));
	BOOST_CHECK(immersive_audio_is_dolby(new_test_film("atmos_reference_signal_name_none", { content_factory("test/data/flat_red.png")[0] })));
}


/** Atmos without a reference signal (an IAB file, say) gives silence: the normal mix is never heard */
BOOST_AUTO_TEST_CASE(atmos_reference_signal_none_test)
{
	auto const path = boost::filesystem::path("build/test/atmos_reference_signal_none.mxf");
	boost::filesystem::remove(path);
	dcp::AtmosAsset asset({24, 1}, 0, 16, 128, 0);
	auto writer = asset.start_write(path);
	vector<uint8_t> const frame = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x01, 0x00 };
	for (int i = 0; i < 24; ++i) {
		writer->write(frame.data(), frame.size());
	}
	writer->finalize();

	auto picture = content_factory("test/data/flat_red.png")[0];
	auto sound = content_factory("test/data/C.wav")[0];
	auto film = new_test_film("atmos_reference_signal_none", { picture, content_factory(path)[0], sound });
	picture->video->set_length(24);

	auto reference = play(film, true);
	BOOST_REQUIRE_GE(reference->frames(), 48000);
	for (int channel = 0; channel < reference->channels(); ++channel) {
		for (int i = 0; i < 48000; ++i) {
			BOOST_REQUIRE_EQUAL(reference->data(channel)[i], 0);
		}
	}
}
