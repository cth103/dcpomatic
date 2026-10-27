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
#include "lib/audio_processor.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/signal_manager.h"
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
#include <algorithm>
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
write_wav(boost::filesystem::path path, bool extensible, int channels = 8, uint32_t mask = 0)
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
		u32(mask ? mask : (channels == 8 ? 0x63f : (1U << channels) - 1));
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

	/* A 5.1 file with its channel mask names L, R, C, LFE and the back pair, which is where 1:1 puts them */
	auto const five_one = boost::filesystem::path("build/test/ffmpeg_audio_7_1_default_mapping_5_1.wav");
	write_wav(five_one, true, 6);
	std::vector<int> const five_one_outputs = { 0, 1, 2, 3, 4, 5 };
	auto const five_one_result = mapped(five_one, "ffmpeg_audio_7_1_default_mapping_5_1", 6);
	BOOST_CHECK_EQUAL_COLLECTIONS(five_one_result.begin(), five_one_result.end(), five_one_outputs.begin(), five_one_outputs.end());

	/* A mask that names fewer channels than the file has (here 5 of 8) can not be trusted to say which is which,
	 * so the mapping stays 1:1
	 */
	auto const inconsistent = boost::filesystem::path("build/test/ffmpeg_audio_7_1_default_mapping_inconsistent.wav");
	write_wav(inconsistent, true, 8, 0x1f);
	auto const inconsistent_result = mapped(inconsistent, "ffmpeg_audio_7_1_default_mapping_inconsistent", 6);
	BOOST_CHECK_EQUAL_COLLECTIONS(inconsistent_result.begin(), inconsistent_result.end(), one_to_one.begin(), one_to_one.end());
}


/** A file with 12 or more channels is mapped 1:1, which already reaches Lrs and Rrs on its 11th and 12th
 *  channels, so adding it does not change the number of channels of the film.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_many_channels_keep_film_channels)
{
	boost::filesystem::create_directories("build/test");

	for (auto channels: { 12, 13, 14, 15, 16 }) {
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


static
std::vector<string>
hints_for(std::shared_ptr<Film> film)
{
	std::vector<string> hints;
	Hints checker(film);
	/* None of these tests need the audio analysis, and it is slow */
	checker.disable_audio_analysis();
	checker.Hint.connect([&hints](string hint) { hints.push_back(hint); });
	checker.start();
	checker.join();
	while (signal_manager->ui_idle()) {}
	checker.rethrow();
	return hints;
}


/** @return true if one of hints is the one about a channel layout that does not match a cinema's */
static
bool
has_layout_hint(std::vector<string> const& hints)
{
	return std::find_if(hints.begin(), hints.end(), [](string const& hint) {
		return hint.find("which is not 5.1 or 7.1 as used in cinemas") != string::npos;
	}) != hints.end();
}


/** 8 channel layouts other than the cinema 7.1 (7.1(wide), 7.1(wide-side), 5.1.2, octagonal, cube) are mapped 1:1,
 *  with a hint to check the mapping; the layout is kept in the metadata, so the hint is still given after the project
 *  has been read back.  The cinema 7.1 gives no hint.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_non_cinema_7_1_hint)
{
	boost::filesystem::create_directories("build/test");

	for (auto mask: { 0xffU, 0x6cfU, 0x503fU, 0x737U, 0x2d033U }) {
		auto const name = fmt::format("ffmpeg_audio_non_cinema_7_1_hint_{:x}", mask);
		auto const path = boost::filesystem::path("build/test/" + name + ".wav");
		write_wav(path, true, 8, mask);
		auto content = content_factory(path)[0];
		auto film = new_test_film(name, { content });
		BOOST_CHECK_EQUAL(film->audio_channels(), 6);
		auto const mapping = content->audio->mapping();
		for (int input = 0; input < 8; ++input) {
			BOOST_CHECK_MESSAGE(mapping.get(input, input) > 0, "channel " << input << " is not mapped 1:1");
		}
		BOOST_CHECK_MESSAGE(has_layout_hint(hints_for(film)), "no hint for mask " << std::hex << mask);

		film->write_metadata();
		auto reloaded = std::make_shared<Film>(film->directory().get());
		reloaded->read_metadata();
		BOOST_CHECK_MESSAGE(has_layout_hint(hints_for(reloaded)), "no hint after reading back for mask " << std::hex << mask);

		/* An audio processor does its own mapping, so there is nothing to check */
		film->set_audio_processor(AudioProcessor::from_id("mid-side-decoder"));
		BOOST_CHECK_MESSAGE(!has_layout_hint(hints_for(film)), "hint with an audio processor for mask " << std::hex << mask);
	}

	auto const cinema = boost::filesystem::path("build/test/ffmpeg_audio_non_cinema_7_1_hint_cinema.wav");
	write_wav(cinema, true);
	auto film = new_test_film("ffmpeg_audio_non_cinema_7_1_hint_cinema", { content_factory(cinema)[0] });
	BOOST_CHECK(!has_layout_hint(hints_for(film)));
}


/** A 6 channel file whose mask is 5.1 or 5.1(side), i.e. L, R, C, LFE and two surrounds, is mapped by its mask,
 *  which is 1:1, with no hint.  Any other 6 channel mask (6.0, 6.0(front), 3.1.2, hexagonal) is mapped 1:1 too,
 *  with a hint to check the mapping.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_six_channel_layouts)
{
	boost::filesystem::create_directories("build/test");

	struct Case {
		uint32_t mask;
		bool hint;
	};

	for (auto c: { Case{0x3f, false}, Case{0x60f, false}, Case{0x707, true}, Case{0x6c3, true}, Case{0x500f, true}, Case{0x137, true} }) {
		auto const name = fmt::format("ffmpeg_audio_six_channel_layouts_{:x}", c.mask);
		auto const path = boost::filesystem::path("build/test/" + name + ".wav");
		write_wav(path, true, 6, c.mask);
		auto content = content_factory(path)[0];
		auto film = new_test_film(name, { content });
		BOOST_CHECK_EQUAL(film->audio_channels(), 6);
		auto const mapping = content->audio->mapping();
		for (int input = 0; input < 6; ++input) {
			BOOST_CHECK_MESSAGE(mapping.get(input, input) > 0, "channel " << input << " of mask " << std::hex << c.mask << " is not mapped 1:1");
		}
		BOOST_CHECK_MESSAGE(has_layout_hint(hints_for(film)) == c.hint, "wrong hint for mask " << std::hex << c.mask);
	}
}


/** A 7.1 file raises a film with fewer than 12 channels to 12, but does not take channels away from a film that
 *  already has 12 or more.
 */
BOOST_AUTO_TEST_CASE(ffmpeg_audio_7_1_does_not_lower_channels)
{
	boost::filesystem::create_directories("build/test");
	auto const path = boost::filesystem::path("build/test/ffmpeg_audio_7_1_does_not_lower_channels.wav");
	write_wav(path, true);

	for (auto channels: { 12, 14, 16 }) {
		auto film = new_test_film(fmt::format("ffmpeg_audio_7_1_does_not_lower_channels_{}", channels));
		film->set_audio_channels(channels);
		film->examine_and_add_content(content_factory(path));
		BOOST_REQUIRE(!wait_for_jobs());
		BOOST_CHECK_EQUAL(film->audio_channels(), channels);
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
