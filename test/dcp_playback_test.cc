/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/butler.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/player.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::make_shared;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


/** Simulate the work that the player does, for profiling */
BOOST_AUTO_TEST_CASE (dcp_playback_test)
{
	auto film = new_test_film ("dcp_playback_test");
	auto content = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	auto butler = std::make_shared<Butler>(
		film,
		make_shared<Player>(film, Image::Alignment::PADDED),
		AudioMapping(6, 6),
		6,
		bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24),
		VideoRange::FULL,
		Image::Alignment::PADDED,
		true,
		false
		);

	std::vector<float> audio_buffer(2000 * 6);
	while (true) {
		auto p = butler->get_video (true, 0);
		if (!p.first) {
			break;
		}
		/* assuming DCP is 24fps/48kHz */
		butler->get_audio (audio_buffer.data(), 2000);
		p.first->image(bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), VideoRange::FULL, true);
	}
}
