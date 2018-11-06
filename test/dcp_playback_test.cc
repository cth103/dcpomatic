/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/butler.h"
#include "lib/player.h"
#include "lib/dcp_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::pair;
using boost::shared_ptr;
using boost::optional;

/** Simulate the work that the player does, for profiling */
BOOST_AUTO_TEST_CASE (dcp_playback_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_playback_test");
	shared_ptr<DCPContent> content (new DCPContent(film, private_data / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	shared_ptr<Butler> butler (new Butler(shared_ptr<Player>(new Player(film, film->playlist())), shared_ptr<Log>(), AudioMapping(6, 6), 6));
	float* audio_buffer = new float[2000*6];
	while (true) {
		pair<shared_ptr<PlayerVideo>, DCPTime> p = butler->get_video ();
		if (!p.first) {
			break;
		}
		/* assuming DCP is 24fps/48kHz */
		butler->get_audio (audio_buffer, 2000);
		p.first->image(optional<dcp::NoteHandler>(), bind(&PlayerVideo::always_rgb, _1), false, true);
	}
	delete[] audio_buffer;
}
