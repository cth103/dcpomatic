/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/dcp_video.h"
#include "lib/decoder.h"
#include "lib/encode_server.h"
#include "lib/encode_server_description.h"
#include "lib/exceptions.h"
#include "lib/file_log.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include "lib/video_decoder.h"
#include <getopt.h>
#include <exception>
#include <iomanip>
#include <iostream>


using std::cerr;
using std::cout;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::ArrayData;


static shared_ptr<Film> film;
static EncodeServerDescription* server;
static int frame_count = 0;


void
process_video (shared_ptr<PlayerVideo> pvf)
{
	auto local = make_shared<DCPVideo>(pvf, frame_count, film->video_frame_rate(), 250000000, Resolution::TWO_K);
	auto remote = make_shared<DCPVideo>(pvf, frame_count, film->video_frame_rate(), 250000000, Resolution::TWO_K);

	cout << "Frame " << frame_count << ": ";
	cout.flush ();

	++frame_count;

	auto local_encoded = local->encode_locally ();
	ArrayData remote_encoded;

	string remote_error;
	try {
		remote_encoded = remote->encode_remotely (*server);
	} catch (NetworkError& e) {
		remote_error = e.what ();
	}

	if (!remote_error.empty()) {
		cout << "\033[0;31mnetwork problem: " << remote_error << "\033[0m\n";
		return;
	}

	if (local_encoded.size() != remote_encoded.size()) {
		cout << "\033[0;31msizes differ\033[0m\n";
		return;
	}

	auto p = local_encoded.data();
	auto q = remote_encoded.data();
	for (int i = 0; i < local_encoded.size(); ++i) {
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
	boost::filesystem::path film_dir;
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

	if (server_host.empty() || film_dir.string().empty()) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	dcpomatic_setup ();

	try {
		server = new EncodeServerDescription (server_host, 1, SERVER_LINK_VERSION);
		film = make_shared<Film>(film_dir);
		film->read_metadata ();

		auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
		player->Video.connect (bind(&process_video, _1));
		while (!player->pass ()) {}
	} catch (std::exception& e) {
		cerr << "Error: " << e.what() << "\n";
	}

	return 0;
}
