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
#include "lib/film.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_subtitle_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


BOOST_AUTO_TEST_CASE (test_subtitle_timing_with_frame_rate_change)
{
	Cleanup cl;

	using boost::filesystem::path;

	constexpr auto content_frame_rate = 29.976f;
	const std::string name = "test_subtitle_timing_with_frame_rate_change";

	auto picture = content_factory("test/data/flat_red.png")[0];
	auto sub = content_factory("test/data/hour.srt")[0];
	sub->text.front()->set_language(dcp::LanguageTag("en"));

	auto film = new_test_film2(name, { picture, sub }, &cl);
	picture->set_video_frame_rate(film, content_frame_rate);
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

	auto subs = reel->main_subtitle()->asset()->subtitles();
	int index = 0;
	for (auto i: subs) {
		auto error = std::abs(i->in().as_seconds() - (index * content_frame_rate / dcp_frame_rate));
		BOOST_CHECK (error < (1.0f / dcp_frame_rate));
		++index;
	}

	cl.run();
}

