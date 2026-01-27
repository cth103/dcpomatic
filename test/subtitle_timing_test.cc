/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/content_text.h"
#include "lib/dcpomatic_time.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/text_content.h"
#include "lib/text_decoder.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_text_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::dynamic_pointer_cast;


BOOST_AUTO_TEST_CASE (test_subtitle_timing_with_frame_rate_change)
{
	Cleanup cl;

	using boost::filesystem::path;

	constexpr auto content_frame_rate = 29.976f;
	const std::string name = "test_subtitle_timing_with_frame_rate_change";

	auto picture = content_factory("test/data/flat_red.png")[0];
	auto sub = content_factory("test/data/hour.srt")[0];
	sub->text.front()->set_language(dcp::LanguageTag("en"));

	auto film = new_test_film(name, { picture, sub }, &cl);
	film->set_video_bit_rate(VideoEncoding::JPEG2000, 10000000);
	picture->set_video_frame_rate(content_frame_rate);
	auto const dcp_frame_rate = film->video_frame_rate();

	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME, dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K });

	dcp::DCP dcp(path("build/test") / name / film->dcp_name());
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	BOOST_REQUIRE(reel->main_subtitle());
	BOOST_REQUIRE(reel->main_subtitle()->asset());

	auto subs = reel->main_subtitle()->asset()->texts();
	int index = 0;
	for (auto i: subs) {
		auto error = std::abs(i->in().as_seconds() - (index * content_frame_rate / dcp_frame_rate));
		BOOST_CHECK (error < (1.0f / dcp_frame_rate));
		++index;
	}

	cl.run();
}


BOOST_AUTO_TEST_CASE(dvb_subtitles_replace_the_last)
{
	/* roh.mkv contains subtitles that come out of FFmpeg with incorrect stop times (30s
	 * after the start, which seems to be some kind of DVB "standard" timeout).
	 * Between actual subtitles it contains blanks that are apparently supposed to clear
	 * the previous subtitle.  Make sure that happens.
	 */
	auto content = content_factory(TestPaths::private_data() / "roh.mkv");
	BOOST_REQUIRE(!content.empty());
	auto film = new_test_film("dvb_subtitles_replace_the_last", { content[0] });

	FFmpegDecoder decoder(film, dynamic_pointer_cast<FFmpegContent>(content[0]), false);
	BOOST_REQUIRE(!decoder.text.empty());

	struct Event {
		std::string type;
		dcpomatic::ContentTime time;

		bool operator==(Event const& other) const {
			return type == other.type && time == other.time;
		}
	};

	std::vector<Event> events;

	auto start = [&events](ContentBitmapText text) {
		events.push_back({"start", text.from()});
	};

	auto stop = [&events](dcpomatic::ContentTime time) {
		if (!events.empty() && events.back().type == "stop") {
			/* We'll get a bad (too-late) stop time, then the correct one
			 * when the "clearing" subtitle arrives.
			 */
			events.pop_back();
		}
		events.push_back({"stop", time});
	};

	decoder.text.front()->BitmapStart.connect(start);
	decoder.text.front()->Stop.connect(stop);

	while (!decoder.pass()) {}

	using dcpomatic::ContentTime;

	std::vector<Event> correct = {
		{ "start", ContentTime(439872) },  // 4.582000s     actual subtitle #1
		{ "stop",  ContentTime(998400) },  // 10.400000s    stop caused by incoming blank
		{ "start", ContentTime(998400) },  // 10.400000s    blank
		{ "stop",  ContentTime(1141248) }, // 11.888000s    stop caused by incoming subtitle #2
		{ "start", ContentTime(1141248) }, // 11.888000s    subtitle #2
		{ "stop",  ContentTime(1455936) }, // 15.166000s    ...
		{ "start", ContentTime(1455936) }, // 15.166000s
		{ "stop",  ContentTime(1626816) }, // 16.946000s
		{ "start", ContentTime(1626816) }, // 16.946000s
	};

	BOOST_REQUIRE(events.size() > correct.size());
	BOOST_CHECK(std::vector<Event>(events.begin(), events.begin() + correct.size()) == correct);
}

