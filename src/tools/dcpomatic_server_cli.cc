/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "lib/server.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include "lib/config.h"
#include "lib/dcp_video.h"
#include "lib/exceptions.h"
#include "lib/util.h"
#include "lib/config.h"
#include "lib/scaler.h"
#include "lib/image.h"
#include "lib/log.h"
#include "lib/version.h"

using std::cerr;
using std::string;
using std::cout;
using boost::shared_ptr;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION]\n"
	     << "  -v, --version      show DCP-o-matic version\n"
	     << "  -h, --help         show this help\n"
	     << "  -t, --threads      number of parallel encoding threads to use\n"
	     << "  --verbose          be verbose to stdout\n"
	     << "  --log              write a log file of activity\n";
}

int
main (int argc, char* argv[])
{
	int num_threads = Config::instance()->num_local_encoding_threads ();
	bool verbose = false;
	bool write_log = false;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "threads", required_argument, 0, 't'},
			{ "verbose", no_argument, 0, 'A'},
			{ "log", no_argument, 0, 'B'},
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "vht:AB", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'v':
			cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 't':
			num_threads = atoi (optarg);
			break;
		case 'A':
			verbose = true;
			break;
		case 'B':
			write_log = true;
			break;
		}
	}

	Scaler::setup_scalers ();
	shared_ptr<Log> log;
	if (write_log) {
		log.reset (new FileLog ("dcpomatic_server_cli.log"));
	} else {
		log.reset (new NullLog);
	}
	
	Server server (log, verbose);
	
	try {
		server.run (num_threads);
	} catch (boost::system::system_error& e) {
		if (e.code() == boost::system::errc::address_in_use) {
			cerr << argv[0] << ": address already in use.  Is another DCP-o-matic server instance already running?\n";
			exit (EXIT_FAILURE);
		}
		cerr << argv[0] << ": " << e.what() << "\n";
	}
	return 0;
}
