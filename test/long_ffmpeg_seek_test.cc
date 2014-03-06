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

#include <boost/test/unit_test.hpp>
#include "lib/player.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "test.h"

using std::cout;
using std::string;
using std::stringstream;
using boost::shared_ptr;

#define LONG_FFMPEG_SEEK_TEST_DEBUG 1

boost::optional<DCPTime> first_video;
boost::optional<DCPTime> first_audio;

static void
process_video (shared_ptr<PlayerImage>, Eyes, ColourConversion, bool, DCPTime t)
{
	if (!first_video) {
		first_video = t;
	}
}

static void
process_audio (shared_ptr<const AudioBuffers>, DCPTime t)
{
	if (!first_audio) {
		first_audio = t;
	}
}

static string
print_time (DCPTime t, float fps)
{
	stringstream s;
	s << t << " " << t.seconds() << "s " << t.frames(fps) << "f";
	return s.str ();
}

static void
check (shared_ptr<Player> p, DCPTime t)
{
	first_video.reset ();
	first_audio.reset ();

#if LONG_FFMPEG_SEEK_TEST_DEBUG == 1
	cout << "\n-- Seek to " << print_time (t, 24) << "\n";
#endif	
	
	p->seek (t, true);
	while (!first_video || !first_audio) {
		p->pass ();
	}

#if LONG_FFMPEG_SEEK_TEST_DEBUG == 1
	cout << "First video " << print_time (first_video.get(), 24) << "\n";
	cout << "First audio " << print_time (first_audio.get(), 24) << "\n";
#endif	
	
	BOOST_CHECK (first_video.get() >= t);
	BOOST_CHECK (first_audio.get() >= t);
}

BOOST_AUTO_TEST_CASE (long_ffmpeg_seek_test)
{
	shared_ptr<Film> film = new_test_film ("long_ffmpeg_audio_test");
	film->set_name ("long_ffmpeg_audio_test");
	film->set_container (Ratio::from_id ("185"));
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/long_data/dolby_aurora.vob"));
	c->set_scale (VideoContentScale (Ratio::from_id ("185")));
	film->examine_and_add_content (c);

	wait_for_jobs ();

	shared_ptr<Player> player = film->make_player ();
	player->Video.connect (boost::bind (&process_video, _1, _2, _3, _4, _5));
	player->Audio.connect (boost::bind (&process_audio, _1, _2));

	for (float i = 0; i < 10; i += 0.1) {
		check (player, DCPTime::from_seconds (i));
	}
}


