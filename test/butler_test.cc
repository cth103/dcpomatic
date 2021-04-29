/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/content_factory.h"
#include "lib/audio_mapping.h"
#include "lib/player.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (butler_test1)
{
	auto film = new_test_film ("butler_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("FTR"));
	film->set_name ("butler_test1");
	film->set_container (Ratio::from_id ("185"));

	auto video = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (video);
	auto audio = content_factory("test/data/staircase.wav").front();
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs ());

	film->set_audio_channels (6);

	/* This is the map of the player output (5.1) to the butler output (also 5.1) */
	auto map = AudioMapping (6, 6);
	for (int i = 0; i < 6; ++i) {
		map.set (i, i, 1);
	}

	Butler butler (film, make_shared<Player>(film), map, 6, bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), VideoRange::FULL, false, false);

	BOOST_CHECK (butler.get_video(true, 0).second == DCPTime());
	BOOST_CHECK (butler.get_video(true, 0).second == DCPTime::from_frames(1, 24));
	BOOST_CHECK (butler.get_video(true, 0).second == DCPTime::from_frames(2, 24));
	/* XXX: check the frame contents */

	float buffer[256 * 6];
	BOOST_REQUIRE (butler.get_audio(buffer, 256) == DCPTime());
	for (int i = 0; i < 256; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 0], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 1], 0);
		BOOST_REQUIRE_CLOSE (buffer[i * 6 + 2], i / 32768.0f, 0.1);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 3], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 4], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 5], 0);
	}
}
