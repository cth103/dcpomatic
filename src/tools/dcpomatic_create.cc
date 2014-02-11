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

#include <string>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <getopt.h>
#include <boost/filesystem.hpp>
#include "lib/version.h"
#include "lib/film.h"
#include "lib/util.h"
#include "lib/content_factory.h"
#include "lib/job_manager.h"
#include "lib/ui_signaller.h"
#include "lib/job.h"

using std::string;
using std::cout;
using std::cerr;
using std::list;
using std::exception;
using boost::shared_ptr;

static void
help (string n)
{
	cerr << "Create a film directory (ready for making a DCP) from some content files.\n"
	     << "Syntax: " << n << " [OPTION] <CONTENT> [<CONTENT> ...]\n"
	     << "  -v, --version   show DCP-o-matic version\n"
	     << "  -h, --help      show this help\n"
	     << "  -n, --name      film name\n"
	     << "  -o, --output    output directory (required)\n";
}

int
main (int argc, char* argv[])
{
	string name;
	boost::filesystem::path output;
	
	int option_index = 0;
	while (1) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "name", required_argument, 0, 'n'},
			{ "output", required_argument, 0, 'o'},
			{ 0, 0, 0, 0}
		};

		int c = getopt_long (argc, argv, "vhn:o:", long_options, &option_index);
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
		case 'n':
			name = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		}
	}

	if (optind > argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	if (output.empty ()) {
		cerr << "Missing required option -o or --output.\n"
		     << "Use " << argv[0] << " --help for help.\n";
		exit (EXIT_FAILURE);
	}

	dcpomatic_setup ();
	ui_signaller = new UISignaller ();

	try {
		shared_ptr<Film> film (new Film (output));
		if (!name.empty ()) {
			film->set_name (name);
		}
		
		for (int i = optind; i < argc; ++i) {
			film->examine_and_add_content (content_factory (film, argv[i]));
		}
		
		JobManager* jm = JobManager::instance ();
		while (jm->work_to_do ()) {
			ui_signaller->ui_idle ();
		}
		
		if (jm->errors ()) {
			list<shared_ptr<Job> > jobs = jm->get ();
			for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
				if ((*i)->finished_in_error ()) {
					cerr << (*i)->error_summary () << "\n"
					     << (*i)->error_details () << "\n";
				}
			}
			exit (EXIT_FAILURE);
		}
		
		film->write_metadata ();
	} catch (exception& e) {
		cerr << argv[0] << ": " << e.what() << "\n";
		exit (EXIT_FAILURE);
	}
		
	return 0;
}
