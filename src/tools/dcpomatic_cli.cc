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

#include "lib/film.h"
#include "lib/filter.h"
#include "lib/transcode_job.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/cross.h"
#include "lib/config.h"
#include "lib/log.h"
#include "lib/signal_manager.h"
#include "lib/encode_server_finder.h"
#include "lib/json_server.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include <dcp/version.h>
#include <boost/foreach.hpp>
#include <getopt.h>
#include <iostream>
#include <iomanip>

using std::string;
using std::cerr;
using std::cout;
using std::vector;
using std::pair;
using std::setw;
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] [<FILM>]\n"
	     << "  -v, --version      show DCP-o-matic version\n"
	     << "  -h, --help         show this help\n"
	     << "  -f, --flags        show flags passed to C++ compiler on build\n"
	     << "  -n, --no-progress  do not print progress to stdout\n"
	     << "  -r, --no-remote    do not use any remote servers\n"
	     << "  -t, --threads      specify number of local encoding threads (overriding configuration)\n"
	     << "  -j, --json <port>  run a JSON server on the specified port\n"
	     << "  -k, --keep-going   keep running even when the job is complete\n"
	     << "  -s, --servers      just display a list of encoding servers that DCP-o-matic is configured to use; don't encode\n"
	     << "  -d, --dcp-path     echo DCP's path to stdout on successful completion (implies -n)\n"
	     << "      --dump         just dump a summary of the film's settings; don't encode\n"
	     << "\n"
	     << "<FILM> is the film directory.\n";
}

static void
print_dump (shared_ptr<Film> film)
{
	cout << film->dcp_name (true) << "\n"
	     << film->container()->container_nickname() << " at " << ((film->resolution() == RESOLUTION_2K) ? "2K" : "4K") << "\n"
	     << (film->j2k_bandwidth() / 1000000) << "Mbit/s" << "\n"
	     << "Output " << film->video_frame_rate() << "fps " << (film->three_d() ? "3D" : "2D") << " " << (film->audio_frame_rate() / 1000) << "kHz\n"
	     << (film->interop() ? "Inter-Op" : "SMPTE") << " " << (film->encrypted() ? "encrypted" : "unencrypted") << "\n";

	BOOST_FOREACH (shared_ptr<Content> c, film->content ()) {
		cout << "\n"
		     << c->path(0) << "\n"
		     << "\tat " << c->position().seconds ()
		     << " length " << c->full_length().seconds ()
		     << " start trim " << c->trim_start().seconds ()
		     << " end trim " << c->trim_end().seconds () << "\n";

		if (c->video) {
			cout << "\t" << c->video->size().width << "x" << c->video->size().height << "\n"
			     << "\t" << c->active_video_frame_rate() << "fps\n"
			     << "\tcrop left " << c->video->left_crop()
			     << " right " << c->video->right_crop()
			     << " top " << c->video->top_crop()
			     << " bottom " << c->video->bottom_crop() << "\n"
			     << "\tscale " << c->video->scale().name() << "\n";
			if (c->video->colour_conversion()) {
				if (c->video->colour_conversion().get().preset()) {
					cout << "\tcolour conversion "
					     << PresetColourConversion::all()[c->video->colour_conversion().get().preset().get()].name
					     << "\n";
				} else {
					cout << "\tcustom colour conversion\n";
				}
			} else {
				cout << "\tno colour conversion\n";
			}

		}

		if (c->audio) {
			cout << "\t" << c->audio->delay() << " delay\n"
			     << "\t" << c->audio->gain() << " gain\n";
		}
	}
}

static void
show_servers ()
{
	while (true) {
		int N = 0;
		list<EncodeServerDescription> servers = EncodeServerFinder::instance()->servers ();

		if (Config::instance()->use_any_servers ()) {
			if (servers.empty ()) {
				cout << "No encoding servers found.\n";
				++N;
			} else {
				cout << std::left << setw(24) << "Host" << " Threads\n";
				++N;
				BOOST_FOREACH (EncodeServerDescription const & i, servers) {
					cout << std::left << setw(24) << i.host_name() << " " << i.threads() << "\n";
					++N;
				}
			}
		} else {
			vector<string> configured_servers = Config::instance()->servers();
			if (configured_servers.empty()) {
				cout << "No configured servers, and DCP-o-matic is not set to search for any server.\n";
				++N;
			} else {
				cout << std::left << setw(24) << "Host" << " Status Threads\n";
				++N;
				BOOST_FOREACH (string const & i, Config::instance()->servers()) {
					cout << std::left << setw(24) << i << " ";
					optional<int> threads;
					BOOST_FOREACH (EncodeServerDescription const & j, servers) {
						if (i == j.host_name()) {
							threads = j.threads();
						}
					}
					if (static_cast<bool>(threads)) {
						cout << "UP     " << threads.get() << "\n";
					} else {
						cout << "DOWN\n";
					}
					++N;
				}
			}
		}


		dcpomatic_sleep (1);

		for (int i = 0; i < N; ++i) {
			cout << "\033[1A\033[2K";
		}
	}
}


int
main (int argc, char* argv[])
{
	boost::filesystem::path film_dir;
	bool progress = true;
	bool no_remote = false;
	optional<int> threads;
	optional<int> json_port;
	bool keep_going = false;
	bool dump = false;
	bool servers = false;
	bool dcp_path = false;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "flags", no_argument, 0, 'f'},
			{ "no-progress", no_argument, 0, 'n'},
			{ "no-remote", no_argument, 0, 'r'},
			{ "threads", required_argument, 0, 't'},
			{ "json", required_argument, 0, 'j'},
			{ "keep-going", no_argument, 0, 'k' },
			{ "servers", no_argument, 0, 's' },
			{ "dcp-path", no_argument, 0, 'd' },
			/* Just using A, B, C ... from here on */
			{ "dump", no_argument, 0, 'A' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "vhfnrt:j:kAsd", long_options, &option_index);

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
		case 'f':
			cout << dcpomatic_cxx_flags << "\n";
			exit (EXIT_SUCCESS);
		case 'n':
			progress = false;
			break;
		case 'r':
			no_remote = true;
			break;
		case 't':
			threads = atoi (optarg);
			break;
		case 'j':
			json_port = atoi (optarg);
			break;
		case 'k':
			keep_going = true;
			break;
		case 'A':
			dump = true;
			break;
		case 's':
			servers = true;
			break;
		case 'd':
			dcp_path = true;
			progress = false;
			break;
		}
	}

	if (servers) {
		show_servers ();
		exit (EXIT_SUCCESS);
	}

	if (optind >= argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	film_dir = argv[optind];

	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();
	signal_manager = new SignalManager ();

	if (no_remote) {
		EncodeServerFinder::instance()->stop ();
	}

	if (json_port) {
		new JSONServer (json_port.get ());
	}

	if (threads) {
		Config::instance()->set_master_encoding_threads (threads.get ());
	}

	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir));
		film->read_metadata ();
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir.string() << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	if (dump) {
		print_dump (film);
		exit (EXIT_SUCCESS);
	}

	ContentList content = film->content ();
	for (ContentList::const_iterator i = content.begin(); i != content.end(); ++i) {
		vector<boost::filesystem::path> paths = (*i)->paths ();
		for (vector<boost::filesystem::path>::const_iterator j = paths.begin(); j != paths.end(); ++j) {
			if (!boost::filesystem::exists (*j)) {
				cerr << argv[0] << ": content file " << *j << " not found.\n";
				exit (EXIT_FAILURE);
			}
		}
	}

	if (progress) {
		cout << "\nMaking DCP for " << film->name() << "\n";
	}

	film->make_dcp ();

	bool should_stop = false;
	bool first = true;
	bool error = false;
	while (!should_stop) {

		dcpomatic_sleep (5);

		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();

		if (!first && progress) {
			for (size_t i = 0; i < jobs.size(); ++i) {
				cout << "\033[1A\033[2K";
			}
			cout.flush ();
		}

		first = false;

		int unfinished = 0;
		int finished_in_error = 0;

		BOOST_FOREACH (shared_ptr<Job> i, jobs) {
			if (progress) {
				cout << i->name();
				if (!i->sub_name().empty()) {
					cout << "; " << i->sub_name();
				}
				cout << ": ";

				if (i->progress ()) {
					cout << i->status() << "			    \n";
				} else {
					cout << ": Running	     \n";
				}
			}

			if (!i->finished ()) {
				++unfinished;
			}

			if (i->finished_in_error ()) {
				++finished_in_error;
				error = true;
			}

			if (!progress && i->finished_in_error ()) {
				/* We won't see this error if we haven't been showing progress,
				   so show it now.
				*/
				cout << i->status() << "\n";
			}
		}

		if (unfinished == 0 || finished_in_error != 0) {
			should_stop = true;
		}
	}

	if (keep_going) {
		while (true) {
			dcpomatic_sleep (3600);
		}
	}

	/* This is just to stop valgrind reporting leaks due to JobManager
	   indirectly holding onto codecs.
	*/
	JobManager::drop ();

	EncodeServerFinder::drop ();

	if (dcp_path && !error) {
		cout << film->dir (film->dcp_name (false)).string() << "\n";
	}

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
