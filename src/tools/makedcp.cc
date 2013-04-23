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

#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <libdcp/test_mode.h>
#include <libdcp/version.h>
#include "format.h"
#include "film.h"
#include "filter.h"
#include "transcode_job.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "util.h"
#include "scaler.h"
#include "version.h"
#include "cross.h"
#include "config.h"
#include "log.h"

using std::string;
using std::cerr;
using std::cout;
using std::vector;
using std::pair;
using std::list;
using boost::shared_ptr;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <FILM>\n"
	     << "  -v, --version      show DVD-o-matic version\n"
	     << "  -h, --help         show this help\n"
	     << "  -d, --deps         list DVD-o-matic dependency details and quit\n"
	     << "  -t, --test         run in test mode (repeatable UUID generation, timestamps etc.)\n"
	     << "  -n, --no-progress  do not print progress to stdout\n"
	     << "  -r, --no-remote    do not use any remote servers\n"
	     << "\n"
	     << "<FILM> is the film directory.\n";
}

int
main (int argc, char* argv[])
{
	string film_dir;
	bool test_mode = false;
	bool progress = true;
	bool no_remote = false;
	int log_level = 0;

	int option_index = 0;
	while (1) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "deps", no_argument, 0, 'd'},
			{ "test", no_argument, 0, 't'},
			{ "no-progress", no_argument, 0, 'n'},
			{ "no-remote", no_argument, 0, 'r'},
			{ "log-level", required_argument, 0, 'l' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "vhdtnrl:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'v':
			cout << "dvdomatic version " << dvdomatic_version << " " << dvdomatic_git_commit << "\n";
			exit (EXIT_SUCCESS);
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'd':
			cout << dependency_version_summary () << "\n";
			exit (EXIT_SUCCESS);
		case 't':
			test_mode = true;
			break;
		case 'n':
			progress = false;
			break;
		case 'r':
			no_remote = true;
			break;
		case 'l':
			log_level = atoi (optarg);
			break;
		}
	}

	if (optind >= argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	film_dir = argv[optind];
			
	dvdomatic_setup ();

	if (no_remote) {
		Config::instance()->set_servers (vector<ServerDescription*> ());
	}

	cout << "DVD-o-matic " << dvdomatic_version << " git " << dvdomatic_git_commit;
	char buf[256];
	if (gethostname (buf, 256) == 0) {
		cout << " on " << buf;
	}
	cout << "\n";

	if (test_mode) {
		libdcp::enable_test_mode ();
		cout << dependency_version_summary() << "\n";
	}

	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir, true));
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	film->log()->set_level ((Log::Level) log_level);

	cout << "\nMaking ";
	if (film->dcp_ab()) {
		cout << "A/B ";
	}
	cout << "DCP for " << film->name() << "\n";
	cout << "Test mode: " << (test_mode ? "yes" : "no") << "\n";
	cout << "Content: " << film->content() << "\n";
	pair<string, string> const f = Filter::ffmpeg_strings (film->filters ());
	cout << "Filters: " << f.first << " " << f.second << "\n";

	film->make_dcp ();

	bool should_stop = false;
	bool first = true;
	bool error = false;
	while (!should_stop) {

		dvdomatic_sleep (5);

		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();

		if (!first && progress) {
			cout << "\033[" << jobs.size() << "A";
			cout.flush ();
		}

		first = false;

		int unfinished = 0;
		int finished_in_error = 0;

		for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
			if (progress) {
				cout << (*i)->name() << ": ";
				
				float const p = (*i)->overall_progress ();
				
				if (p >= 0) {
					cout << (*i)->status() << "                         \n";
				} else {
					cout << ": Running           \n";
				}
			}

			if (!(*i)->finished ()) {
				++unfinished;
			}

			if ((*i)->finished_in_error ()) {
				++finished_in_error;
				error = true;
			}

			if (!progress && (*i)->finished_in_error ()) {
				/* We won't see this error if we haven't been showing progress,
				   so show it now.
				*/
				cout << (*i)->status() << "\n";
			}
		}

		if (unfinished == 0 || finished_in_error != 0) {
			should_stop = true;
		}
	}

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

	  
