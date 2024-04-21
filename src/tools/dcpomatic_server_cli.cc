/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "lib/config.h"
#include "lib/dcp_video.h"
#include "lib/dcpomatic_log.h"
#include "lib/encode_server.h"
#include "lib/exceptions.h"
#include "lib/file_log.h"
#ifdef DCPOMATIC_GROK
#include "lib/grok/context.h"
#endif
#include "lib/image.h"
#include "lib/null_log.h"
#include "lib/util.h"
#include "lib/variant.h"
#include "lib/version.h"
#include <boost/algorithm/string.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <vector>

using std::cerr;
using std::cout;
using std::shared_ptr;
using std::string;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION]\n"
	     << variant::insert_dcpomatic("  -v, --version      show %1 version\n")
	     << "  -h, --help         show this help\n"
	     << "  -t, --threads      number of parallel encoding threads to use\n"
	     << "  --verbose          be verbose to stdout\n"
	     << "  --log              write a log file of activity\n";
}

int
main (int argc, char* argv[])
{
	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();

	int num_threads = Config::instance()->server_encoding_threads ();
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

	if (write_log) {
		dcpomatic_log.reset (new FileLog("dcpomatic_server_cli.log"));
	}

#ifdef DCPOMATIC_GROK
	setMessengerLogger(new grk_plugin::GrokLogger("[GROK] "));
	setup_grok_library_path();
#endif

	EncodeServer server (verbose, num_threads);

	try {
		server.run ();
	} catch (boost::system::system_error& e) {
		if (e.code() == boost::system::errc::address_in_use) {
			cerr << argv[0] << variant::insert_dcpomatic(": address already in use.  Is another %1 server instance already running?\n");
			exit (EXIT_FAILURE);
		}
		cerr << argv[0] << ": " << e.what() << "\n";
	} catch (std::exception& e) {
		cerr << argv[0] << ": failed to start server; " << e.what() << "\n";
	}
	return 0;
}
