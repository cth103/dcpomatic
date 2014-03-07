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
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "test.h"

/* This test needs stuff in Player that is only included in debug mode */
#ifdef DCPOMATIC_DEBUG

using std::cout;
using boost::optional;
using boost::shared_ptr;

struct Video
{
	boost::shared_ptr<Content> content;
	boost::shared_ptr<const Image> image;
	DCPTime time;
};

class PlayerWrapper
{
public:
	PlayerWrapper (shared_ptr<Player> p)
		: _player (p)
	{
		_player->Video.connect (bind (&PlayerWrapper::process_video, this, _1, _2, _5));
	}

	void process_video (shared_ptr<PlayerImage> i, bool, DCPTime t)
	{
		Video v;
		v.content = _player->_last_video;
		v.image = i->image (PIX_FMT_RGB24, false);
		v.time = t;
		_queue.push_front (v);
	}

	optional<Video> get_video ()
	{
		while (_queue.empty() && !_player->pass ()) {}
		if (_queue.empty ()) {
			return optional<Video> ();
		}
		
		Video v = _queue.back ();
		_queue.pop_back ();
		return v;
	}

	void seek (DCPTime t, bool ac)
	{
		_player->seek (t, ac);
		_queue.clear ();
	}

private:
	shared_ptr<Player> _player;
	std::list<Video> _queue;
};

BOOST_AUTO_TEST_CASE (play_test)
{
	shared_ptr<Film> film = new_test_film ("play_test");
	film->set_dcp_content_type (DCPContentType::from_dci_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("play_test");

	shared_ptr<FFmpegContent> A (new FFmpegContent (film, "test/data/red_24.mp4"));
	film->examine_and_add_content (A);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (A->video_length().frames (24), 16);

	shared_ptr<FFmpegContent> B (new FFmpegContent (film, "test/data/red_30.mp4"));
	film->examine_and_add_content (B);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (B->video_length().frames (30), 16);
	
	/* Film should have been set to 25fps */
	BOOST_CHECK_EQUAL (film->video_frame_rate(), 25);

	BOOST_CHECK_EQUAL (A->position(), DCPTime ());
	/* A is 16 frames long at 25 fps */
	BOOST_CHECK_EQUAL (B->position(), DCPTime::from_frames (16, 25));

	shared_ptr<Player> player = film->make_player ();
	PlayerWrapper wrap (player);

	for (int i = 0; i < 32; ++i) {
		optional<Video> v = wrap.get_video ();
		BOOST_CHECK (v);
		if (i < 16) {
			BOOST_CHECK (v.get().content == A);
		} else {
			BOOST_CHECK (v.get().content == B);
		}
	}

	player->seek (DCPTime::from_frames (6, 25), true);
	optional<Video> v = wrap.get_video ();
	BOOST_CHECK (v);
	BOOST_CHECK_EQUAL (v.get().time, DCPTime::from_frames (6, 25));
}

#endif
