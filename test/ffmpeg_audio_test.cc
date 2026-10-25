/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/ffmpeg_audio_test.cc
 *  @brief Test reading audio from an FFmpeg file.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_frame.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/reel.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <map>


using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE (ffmpeg_audio_test)
{
	auto c = make_shared<FFmpegContent> ("test/data/staircase.mov");
	auto film = new_test_film("ffmpeg_audio_test", { c });

	int constexpr audio_channels = 6;

	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels(audio_channels);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	make_and_verify_dcp (film);

	boost::filesystem::path path = "build/test";
	path /= "ffmpeg_audio_test";
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	auto sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_REQUIRE_EQUAL(sound_asset->asset()->channels (), audio_channels);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	while (n < sound_asset->asset()->intrinsic_duration()) {
		auto sound_frame = sound_asset->asset()->start_read()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();
		for (int offset = 0; offset < sound_frame->size(); offset += (3 * sound_asset->asset()->channels())) {
			for (auto channel = 0; channel < audio_channels; ++channel) {
				auto const sample = d[offset + channel * 3 + 1] | (d[offset + channel * 3 + 2] << 8);
				if (channel == 2) {
					/* Input should be on centre */
					BOOST_CHECK_EQUAL(sample, n);
				} else {
					/* Everything else should be silent */
					BOOST_CHECK_EQUAL(sample, 0);
				}
			}
			++n;
		}
	}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to normal */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test2)
{
	auto film = new_test_film("ffmpeg_audio_test2");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	while (!player->pass ()) {}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to fast */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test3)
{
	auto film = new_test_film("ffmpeg_audio_test3");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	player->set_fast ();
	while (!player->pass ()) {}
}


/** Decode a file whose audio previously crashed DCP-o-matic (#1857) */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test4)
{
	auto film = new_test_film("ffmpeg_audio_test4");
	auto content = content_factory(TestPaths::private_data() / "Actuellement aout 2020.wmv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	player->set_fast ();
	BOOST_CHECK_NO_THROW (while (!player->pass()) {});
}



BOOST_AUTO_TEST_CASE(no_audio_length_in_header)
{
	auto content = content_factory(TestPaths::private_data() / "10-seconds.thd");
	auto film = new_test_film("no_audio_length_in_header", content);
	BOOST_CHECK(content[0]->full_length(film) == dcpomatic::DCPTime::from_seconds(10));
}


/** Write one second of 48kHz, 16-bit WAV, in which channel n (from 1) holds the constant n * 1000;
 *  if extensible is true it has a channel mask: the one for 7.1 for 8 channels, otherwise one that names that
 *  many channels in the order of the bits
 */
static void
write_wav(boost::filesystem::path path, bool extensible, int channels = 8)
{
	int const rate = 48000;
	int const data_size = rate * channels * 2;
	int const fmt_size = extensible ? 40 : 16;

	std::ofstream file(path.string(), std::ios::binary);
	auto u16 = [&file](uint16_t v) { file.put(v & 0xff); file.put(v >> 8); };
	auto u32 = [&u16](uint32_t v) { u16(v & 0xffff); u16(v >> 16); };

	file.write("RIFF", 4);
	u32(4 + 8 + fmt_size + 8 + data_size);
	file.write("WAVEfmt ", 8);
	u32(fmt_size);
	u16(extensible ? 0xfffe : 1);
	u16(channels);
	u32(rate);
	u32(rate * channels * 2);
	u16(channels * 2);
	u16(16);
	if (extensible) {
		u16(22);
		u16(16);
		/* FL FR FC LFE BL BR SL SR for 8 channels */
		u32(channels == 8 ? 0x63f : (1U << channels) - 1);
		/* KSDATAFORMAT_SUBTYPE_PCM */
		u32(1);
		u32(0x00100000);
		u32(0xaa000080);
		u32(0x719b3800);
	}
	file.write("data", 4);
	u32(data_size);
	for (int i = 0; i < rate; ++i) {
		for (int channel = 0; channel < channels; ++channel) {
			u16((channel + 1) * 1000);
		}
	}
}


/** An 8-channel file that says it is 7.1 is in the order of its mask: L, R, C, LFE, back pair, side pair.
 *  The back pair is mapped to the rear surrounds (Lrs and Rrs) and the side pair to Lss and Rss,
 *  rather than the last two channels going to HI and VI.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_7_1_default_mapping)
{
	auto mapped = [](boost::filesystem::path path, string name, int film_channels) {
		auto content = content_factory(path)[0];
		auto film = new_test_film(name, { content });
		/* A new film has 6 channels; it needs 12 for the rear surrounds */
		BOOST_CHECK_EQUAL(film->audio_channels(), film_channels);
		auto const mapping = content->audio->mapping();
		std::vector<int> outputs;
		for (int input = 0; input < mapping.input_channels(); ++input) {
			int output = -1;
			for (int o = 0; o < mapping.output_channels(); ++o) {
				if (mapping.get(input, o) > 0) {
					output = o;
				}
			}
			outputs.push_back(output);
		}
		return outputs;
	};

	boost::filesystem::create_directories("build/test");

	auto const seven_one = boost::filesystem::path("build/test/ffmpeg_audio_7_1_default_mapping.wav");
	write_wav(seven_one, true);
	std::vector<int> const cinema = {
		static_cast<int>(dcp::Channel::LEFT),
		static_cast<int>(dcp::Channel::RIGHT),
		static_cast<int>(dcp::Channel::CENTRE),
		static_cast<int>(dcp::Channel::LFE),
		static_cast<int>(dcp::Channel::BSL),
		static_cast<int>(dcp::Channel::BSR),
		static_cast<int>(dcp::Channel::LS),
		static_cast<int>(dcp::Channel::RS)
	};
	auto const result = mapped(seven_one, "ffmpeg_audio_7_1_default_mapping", 12);
	BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), cinema.begin(), cinema.end());

	/* Without the mask we do not know what the channels are, so the mapping is still 1:1 */
	auto const unknown = boost::filesystem::path("build/test/ffmpeg_audio_7_1_default_mapping_no_mask.wav");
	write_wav(unknown, false);
	std::vector<int> const one_to_one = { 0, 1, 2, 3, 4, 5, 6, 7 };
	auto const plain = mapped(unknown, "ffmpeg_audio_7_1_default_mapping_no_mask", 6);
	BOOST_CHECK_EQUAL_COLLECTIONS(plain.begin(), plain.end(), one_to_one.begin(), one_to_one.end());
}


/** A file with 12 or more channels is mapped 1:1, which already reaches Lrs and Rrs on its 11th and 12th
 *  channels, so adding it does not change the number of channels of the film.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_many_channels_keep_film_channels)
{
	boost::filesystem::create_directories("build/test");

	for (auto channels: { 12, 16 }) {
		auto const name = fmt::format("ffmpeg_audio_many_channels_{}", channels);
		auto const path = boost::filesystem::path("build/test/" + name + ".wav");
		write_wav(path, true, channels);
		auto content = content_factory(path)[0];
		auto film = new_test_film(name, { content });
		BOOST_CHECK_EQUAL(film->audio_channels(), 6);
		auto const mapping = content->audio->mapping();
		BOOST_REQUIRE_EQUAL(mapping.input_channels(), channels);
		for (int input = 0; input < channels; ++input) {
			BOOST_CHECK_MESSAGE(mapping.get(input, input) > 0, "channel " << input << " of " << channels << " is not mapped 1:1");
		}
	}
}


/** Every channel of a 7.1 file ends up in the DCP channel that its mask names: the content of each file channel
 *  is a different constant, so a mixed up pair would show.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_7_1_channels_in_dcp)
{
	boost::filesystem::create_directories("build/test");
	auto const path = boost::filesystem::path("build/test/ffmpeg_audio_7_1_channels_in_dcp.wav");
	write_wav(path, true);

	auto picture = content_factory("test/data/flat_red.png")[0];
	auto sound = content_factory(path)[0];
	auto film = new_test_film("ffmpeg_audio_7_1_channels_in_dcp", { picture, sound });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto asset = dcp.cpls()[0]->reels()[0]->main_sound()->asset();
	BOOST_REQUIRE_EQUAL(asset->channels(), 12);

	/* The file channel (from 1) that each DCP channel should carry, or 0 for none */
	std::map<dcp::Channel, int> const expected = {
		{ dcp::Channel::LEFT, 1 },
		{ dcp::Channel::RIGHT, 2 },
		{ dcp::Channel::CENTRE, 3 },
		{ dcp::Channel::LFE, 4 },
		{ dcp::Channel::LS, 7 },
		{ dcp::Channel::RS, 8 },
		{ dcp::Channel::HI, 0 },
		{ dcp::Channel::VI, 0 },
		{ dcp::Channel::BSL, 5 },
		{ dcp::Channel::BSR, 6 }
	};

	auto data = asset->start_read()->get_frame(0);
	for (auto const& channel: expected) {
		/* A 16-bit sample is 8 bits down on the 24 bits of the DCP */
		BOOST_CHECK_EQUAL(data->get(static_cast<int>(channel.first), 1000), channel.second * 1000 * 256);
	}
}


/** A real 7.1 cinema mix (a WAV with the 7.1 channel mask), added with the default
 *  settings, must be heard on all eight channels of the DCP, including the rear surrounds Lrs and Rrs.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_7_1_rear_surrounds_in_dcp)
{
	/* 240 frames of the film Jasmine & Jambo: its picture, and a cut of its 7.1 mix */
	auto picture = content_factory(TestPaths::private_data() / "jasmine_jambo_picture.mp4")[0];
	auto sound = content_factory(TestPaths::private_data() / "jasmine_jambo_7_1.wav")[0];
	auto film = new_test_film("ffmpeg_audio_7_1_rear_surrounds_in_dcp", { picture, sound });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto asset = dcp.cpls()[0]->reels()[0]->main_sound()->asset();
	BOOST_REQUIRE_EQUAL(asset->channels(), 12);

	std::vector<bool> heard(asset->channels(), false);
	auto reader = asset->start_read();
	for (int64_t frame = 0; frame < asset->intrinsic_duration(); ++frame) {
		auto data = reader->get_frame(frame);
		for (int sample = 0; sample < data->samples(); ++sample) {
			for (int channel = 0; channel < asset->channels(); ++channel) {
				if (data->get(channel, sample) != 0) {
					heard[channel] = true;
				}
			}
		}
	}

	for (auto channel: { dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE, dcp::Channel::LS, dcp::Channel::RS, dcp::Channel::BSL, dcp::Channel::BSR }) {
		BOOST_CHECK_MESSAGE(heard[static_cast<int>(channel)], "channel " << static_cast<int>(channel) + 1 << " is silent");
	}
	for (auto channel: { dcp::Channel::HI, dcp::Channel::VI }) {
		BOOST_CHECK_MESSAGE(!heard[static_cast<int>(channel)], "channel " << static_cast<int>(channel) + 1 << " is not silent");
	}
}
