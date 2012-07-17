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
#include "format.h"
#include "film.h"
#include "filter.h"
#include "transcode_job.h"
#include "make_dcp_job.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "util.h"
#include "scaler.h"

using namespace std;
using namespace boost;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [--help] [--deps] [--film <film>]\n";
}

int
main (int argc, char* argv[])
{
	string film_dir;

	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "deps", no_argument, 0, 'd'},
			{ "film", required_argument, 0, 'f'},
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		int c = getopt_long (argc, argv, "hdf:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'd':
			cout << dependency_version_summary () << "\n";
			exit (EXIT_SUCCESS);
		case 'f':
			film_dir = optarg;
			break;
		}
	}

	if (film_dir.empty ()) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}
			
	dvdomatic_setup ();

	Film* film = 0;
	try {
		film = new Film (film_dir, true);
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	cout << "\nMaking ";
	if (film->dcp_ab ()) {
		cout << "A/B ";
	}
	cout << "DCP for " << film->name() << "\n";
	cout << "Content: " << film->content() << "\n";
	pair<string, string> const f = Filter::ffmpeg_strings (film->filters ());
	cout << "Filters: " << f.first << " " << f.second << "\n";

	film->make_dcp (true);

	list<shared_ptr<Job> > jobs = JobManager::instance()->get ();

	bool all_done = false;
	bool first = true;
	while (!all_done) {
		
		sleep (5);
		
		if (!first) {
			cout << "\033[" << jobs.size() << "A";
			cout.flush ();
		}

		first = false;
		
		all_done = true;
		for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
			cout << (*i)->name() << ": ";

			float const p = (*i)->overall_progress ();

			if (p >= 0) {
				cout << (*i)->status() << "                         \n";
			} else {
				cout << ": Running           \n";
			}
			
			if (!(*i)->finished ()) {
				all_done = false;
			}
		}
	}

	return 0;
}

	  
