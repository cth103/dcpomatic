/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/upmixer_a_test.cc
 *  @brief Check the Upmixer A against some reference sound files.
 *  @ingroup selfcontained
 */


#include "lib/audio_buffers.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/upmixer_a.h"
#include "test.h"
#include <sndfile.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static SNDFILE* L;
static SNDFILE* R;
static SNDFILE* C;
static SNDFILE* Lfe;
static SNDFILE* Ls;
static SNDFILE* Rs;


static void
write (shared_ptr<AudioBuffers> b, DCPTime)
{
	sf_write_float (L, b->data(0), b->frames());
	sf_write_float (R, b->data(1), b->frames());
	sf_write_float (C, b->data(2), b->frames());
	sf_write_float (Lfe, b->data(3), b->frames());
	sf_write_float (Ls, b->data(4), b->frames());
	sf_write_float (Rs, b->data(5), b->frames());

}


BOOST_AUTO_TEST_CASE (upmixer_a_test)
{
	auto film = new_test_film ("upmixer_a_test");
	film->set_container (Ratio::from_id("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("TLR"));
	film->set_name ("frobozz");
	film->set_audio_processor (AudioProcessor::from_id("stereo-5.1-upmix-a"));
	auto content = make_shared<FFmpegContent>("test/data/white.wav");
	film->examine_and_add_content (content);

	BOOST_REQUIRE (!wait_for_jobs());

	SF_INFO info;
	info.samplerate = 48000;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	L = sf_open ("build/test/upmixer_a_test/L.wav", SFM_WRITE, &info);
	R = sf_open ("build/test/upmixer_a_test/R.wav", SFM_WRITE, &info);
	C = sf_open ("build/test/upmixer_a_test/C.wav", SFM_WRITE, &info);
	Lfe = sf_open ("build/test/upmixer_a_test/Lfe.wav", SFM_WRITE, &info);
	Ls = sf_open ("build/test/upmixer_a_test/Ls.wav", SFM_WRITE, &info);
	Rs = sf_open ("build/test/upmixer_a_test/Rs.wav", SFM_WRITE, &info);

	auto player = make_shared<Player>(film, false);
	player->Audio.connect (bind (&write, _1, _2));
	while (!player->pass()) {}

	sf_close (L);
	sf_close (R);
	sf_close (C);
	sf_close (Lfe);
	sf_close (Ls);
	sf_close (Rs);

	check_wav_file ("test/data/upmixer_a_test/L.wav", "build/test/upmixer_a_test/L.wav");
	check_wav_file ("test/data/upmixer_a_test/R.wav", "build/test/upmixer_a_test/R.wav");
	check_wav_file ("test/data/upmixer_a_test/C.wav", "build/test/upmixer_a_test/C.wav");
	check_wav_file ("test/data/upmixer_a_test/Lfe.wav", "build/test/upmixer_a_test/Lfe.wav");
	check_wav_file ("test/data/upmixer_a_test/Ls.wav", "build/test/upmixer_a_test/Ls.wav");
	check_wav_file ("test/data/upmixer_a_test/Rs.wav", "build/test/upmixer_a_test/Rs.wav");
}
