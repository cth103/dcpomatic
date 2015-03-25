/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <iostream>
#include <iomanip>
#include <exception>
#include <getopt.h>
#include "lib/ratio.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/util.h"
#include "lib/server.h"
#include "lib/dcp_video.h"
#include "lib/decoder.h"
#include "lib/exceptions.h"
#include "lib/log.h"
#include "lib/video_decoder.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/encoded_data.h"

using std::cout;
using std::cerr;
using std::string;
using std::pair;
using boost::shared_ptr;

static shared_ptr<Film> film;
static ServerDescription* server;
static shared_ptr<FileLog> log_ (new FileLog ("servomatictest.log"));
static int frame_count = 0;

void
process_video (shared_ptr<PlayerVideo> pvf)
{
	shared_ptr<DCPVideo> local  (new DCPVideo (pvf, frame_count, film->video_frame_rate(), 250000000, RESOLUTION_2K, true, log_));
	shared_ptr<DCPVideo> remote (new DCPVideo (pvf, frame_count, film->video_frame_rate(), 250000000, RESOLUTION_2K, true, log_));

	cout << "Frame " << frame_count << ": ";
	cout.flush ();

	++frame_count;

	shared_ptr<EncodedData> local_encoded = local->encode_locally (boost::bind (&Log::dcp_log, log_.get(), _1, _2));
	shared_ptr<EncodedData> remote_encoded;

	string remote_error;
	try {
		remote_encoded = remote->encode_remotely (*server);
	} catch (NetworkError& e) {
		remote_error = e.what ();
	}

	if (!remote_error.empty ()) {
		cout << "\033[0;31mnetwork problem: " << remote_error << "\033[0m\n";
		return;
	}

	if (local_encoded->size() != remote_encoded->size()) {
		cout << "\033[0;31msizes differ\033[0m\n";
		return;
	}
		
	uint8_t* p = local_encoded->data();
	uint8_t* q = remote_encoded->data();
	for (int i = 0; i < local_encoded->size(); ++i) {
		if (*p++ != *q++) {
			cout << "\033[0;31mdata differ\033[0m at byte " << i << "\n";
			return;
		}
	}

	cout << "\033[0;32mgood\033[0m\n";
}

static void
help (string n)
{
	cerr << "Syntax: " << n << " [--help] --film <film> --server <host>\n";
	exit (EXIT_FAILURE);
}

int
main (int argc, char* argv[])
{
	string film_dir;
	string server_host;

	while (true) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "server", required_argument, 0, 's'},
			{ "film", required_argument, 0, 'f'},
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		int c = getopt_long (argc, argv, "hs:f:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 's':
			server_host = optarg;
			break;
		case 'f':
			film_dir = optarg;
			break;
		}
	}
	
	if (server_host.empty() || film_dir.empty()) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	dcpomatic_setup ();

	try {
		server = new ServerDescription (server_host, 1);
		film.reset (new Film (film_dir));
		film->read_metadata ();
		
		shared_ptr<Player> player = film->make_player ();

		DCPTime const frame = DCPTime::from_frames (1, film->video_frame_rate ());
		for (DCPTime t; t < film->length(); t += frame) {
			process_video (player->get_video(t, true).front ());
		}
	} catch (std::exception& e) {
		cerr << "Error: " << e.what() << "\n";
	}

	return 0;
}
