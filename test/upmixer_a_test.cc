/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/test/unit_test.hpp>
#include <sndfile.h>
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/sndfile_content.h"
#include "lib/player.h"
#include "lib/audio_buffers.h"
#include "lib/upmixer_a.h"
#include "test.h"

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (upmixer_a_test)
{
	shared_ptr<Film> film = new_test_film ("upmixer_a_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<SndfileContent> content (new SndfileContent (film, "test/data/white.wav"));
	content->set_audio_processor (AudioProcessor::from_id ("stereo-5.1-upmix-a"));
	film->examine_and_add_content (content, true);

	wait_for_jobs ();

	SF_INFO info;
	info.samplerate = 48000;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	SNDFILE* L = sf_open ("build/test/upmixer_a_test/L.wav", SFM_WRITE, &info);
	SNDFILE* R = sf_open ("build/test/upmixer_a_test/R.wav", SFM_WRITE, &info);
	SNDFILE* C = sf_open ("build/test/upmixer_a_test/C.wav", SFM_WRITE, &info);
	SNDFILE* Lfe = sf_open ("build/test/upmixer_a_test/Lfe.wav", SFM_WRITE, &info);
	SNDFILE* Ls = sf_open ("build/test/upmixer_a_test/Ls.wav", SFM_WRITE, &info);
	SNDFILE* Rs = sf_open ("build/test/upmixer_a_test/Rs.wav", SFM_WRITE, &info);

	shared_ptr<Player> player = film->make_player ();
	for (DCPTime t; t < film->length(); t += DCPTime::from_seconds (1)) {
		shared_ptr<AudioBuffers> b = player->get_audio (t, DCPTime::from_seconds (1), true);
		sf_write_float (L, b->data(0), b->frames());
		sf_write_float (R, b->data(1), b->frames());
		sf_write_float (C, b->data(2), b->frames());
		sf_write_float (Lfe, b->data(3), b->frames());
		sf_write_float (Ls, b->data(4), b->frames());
		sf_write_float (Rs, b->data(5), b->frames());
	}

	sf_close (L);
	sf_close (R);
	sf_close (C);
	sf_close (Lfe);
	sf_close (Ls);
	sf_close (Rs);

	check_audio_file ("test/data/upmixer_a_test/L.wav", "build/test/upmixer_a_test/L.wav");
	check_audio_file ("test/data/upmixer_a_test/R.wav", "build/test/upmixer_a_test/R.wav");
	check_audio_file ("test/data/upmixer_a_test/C.wav", "build/test/upmixer_a_test/C.wav");
	check_audio_file ("test/data/upmixer_a_test/Lfe.wav", "build/test/upmixer_a_test/Lfe.wav");
	check_audio_file ("test/data/upmixer_a_test/Ls.wav", "build/test/upmixer_a_test/Ls.wav");
	check_audio_file ("test/data/upmixer_a_test/Rs.wav", "build/test/upmixer_a_test/Rs.wav");
}
