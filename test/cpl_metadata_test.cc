/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/audio_content.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;


BOOST_AUTO_TEST_CASE(main_sound_configuration_test_51_vi)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto L = content_factory("test/data/L.wav")[0];
	auto R = content_factory("test/data/R.wav")[0];
	auto C = content_factory("test/data/C.wav")[0];
	auto Lfe = content_factory("test/data/Lfe.wav")[0];
	auto Ls = content_factory("test/data/Ls.wav")[0];
	auto Rs = content_factory("test/data/Rs.wav")[0];
	auto VI = content_factory("test/data/sine_440.wav")[0];

	auto film = new_test_film("main_sound_configuration_test_51_vi", { picture, L, R, C, Lfe, Ls, Rs, VI });
	film->set_audio_channels(8);

	auto set_map = [](shared_ptr<Content> content, dcp::Channel channel) {
		auto map = content->audio->mapping();
		map.set(0, channel, 1.0f);
		content->audio->set_mapping(map);
	};

	set_map(L, dcp::Channel::LEFT);
	set_map(R, dcp::Channel::RIGHT);
	set_map(C, dcp::Channel::CENTRE);
	set_map(Lfe, dcp::Channel::LFE);
	set_map(Ls, dcp::Channel::LS);
	set_map(Rs, dcp::Channel::RS);
	set_map(VI, dcp::Channel::VI);

	make_and_verify_dcp(film);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];

	auto msc = cpl->main_sound_configuration();
	BOOST_REQUIRE(msc);

	/* We think this should say 51 not 71 at the start (#2580) */
	BOOST_CHECK_EQUAL(msc->to_string(), "51/L,R,C,LFE,Ls,Rs,-,VIN");
}


BOOST_AUTO_TEST_CASE(main_sound_configuration_test_71)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto L = content_factory("test/data/L.wav")[0];
	auto R = content_factory("test/data/R.wav")[0];
	auto C = content_factory("test/data/C.wav")[0];
	auto Lfe = content_factory("test/data/Lfe.wav")[0];
	auto Ls = content_factory("test/data/Ls.wav")[0];
	auto Rs = content_factory("test/data/Rs.wav")[0];
	auto BsL = content_factory("test/data/Ls.wav")[0];
	auto BsR = content_factory("test/data/Rs.wav")[0];
	auto VI = content_factory("test/data/sine_440.wav")[0];

	auto film = new_test_film("main_sound_configuration_test_51_vi", { picture, L, R, C, Lfe, Ls, Rs, BsL, BsR, VI });
	film->set_audio_channels(12);

	auto set_map = [](shared_ptr<Content> content, dcp::Channel channel) {
		auto map = content->audio->mapping();
		map.set(0, channel, 1.0f);
		content->audio->set_mapping(map);
	};

	set_map(L, dcp::Channel::LEFT);
	set_map(R, dcp::Channel::RIGHT);
	set_map(C, dcp::Channel::CENTRE);
	set_map(Lfe, dcp::Channel::LFE);
	set_map(Ls, dcp::Channel::LS);
	set_map(Rs, dcp::Channel::RS);
	set_map(BsL, dcp::Channel::BSL);
	set_map(BsR, dcp::Channel::BSR);
	set_map(VI, dcp::Channel::VI);

	make_and_verify_dcp(film);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];

	auto msc = cpl->main_sound_configuration();
	BOOST_REQUIRE(msc);

	BOOST_CHECK_EQUAL(msc->to_string(), "71/L,R,C,LFE,Lss,Rss,-,VIN,-,-,Lrs,Rrs");
}
