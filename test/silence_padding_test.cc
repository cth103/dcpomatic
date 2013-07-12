/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <libdcp/cpl.h>
#include <libdcp/dcp.h>
#include <libdcp/sound_asset.h>
#include <libdcp/reel.h>
#include "sndfile_content.h"

using boost::lexical_cast;

static void test_silence_padding (int channels)
{
	string const film_name = "silence_padding_test_" + lexical_cast<string> (channels);
	shared_ptr<Film> film = new_test_film (film_name);
	film->set_dcp_content_type (DCPContentType::from_dci_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name (film_name);

	shared_ptr<SndfileContent> content (new SndfileContent (film, "test/data/staircase.wav"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	film->set_dcp_audio_channels (channels);
	film->make_dcp ();
	wait_for_jobs ();

	boost::filesystem::path path = "build/test";
	path /= film_name;
	path /= film->dcp_name ();
	libdcp::DCP check (path.string ());
	check.read ();

	shared_ptr<const libdcp::SoundAsset> sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_CHECK (sound_asset->channels () == channels);
}

BOOST_AUTO_TEST_CASE (silence_padding_test)
{
	for (int i = 1; i < MAX_AUDIO_CHANNELS; ++i) {
		test_silence_padding (i);
	}
}
