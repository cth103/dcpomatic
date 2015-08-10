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
#include "lib/server_finder.h"
#include "lib/json_server.h"
#include "lib/ratio.h"
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
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <FILM>\n"
	     << "  -v, --version      show DCP-o-matic version\n"
	     << "  -h, --help         show this help\n"
	     << "  -f, --flags        show flags passed to C++ compiler on build\n"
	     << "  -n, --no-progress  do not print progress to stdout\n"
	     << "  -r, --no-remote    do not use any remote servers\n"
	     << "  -j, --json <port>  run a JSON server on the specified port\n"
	     << "  -k, --keep-going   keep running even when the job is complete\n"
	     << "      --dump         just dump a summary of the film's settings; don't encode\n"
	     << "\n"
	     << "<FILM> is the film directory.\n";
}

static void
print_dump (shared_ptr<Film> film)
{
	cout << film->dcp_name (true) << "\n"
	     << film->container()->nickname() << " at " << ((film->resolution() == RESOLUTION_2K) ? "2K" : "4K") << "\n"
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

		shared_ptr<VideoContent> video = dynamic_pointer_cast<VideoContent> (c);
		if (video) {
			cout << "\t" << video->video_size().width << "x" << video->video_size().height << "\n"
			     << "\t" << video->video_frame_rate() << "fps\n"
			     << "\tcrop left " << video->left_crop()
			     << " right " << video->right_crop()
			     << " top " << video->top_crop()
			     << " bottom " << video->bottom_crop() << "\n"
			     << "\tscale " << video->scale().name() << "\n";
			if (video->colour_conversion()) {
				if (video->colour_conversion().get().preset()) {
					cout << "\tcolour conversion "
					     << PresetColourConversion::all()[video->colour_conversion().get().preset().get()].name
					     << "\n";
				} else {
					cout << "\tcustom colour conversion\n";
				}
			} else {
				cout << "\tno colour conversion\n";
			}

		}

		shared_ptr<AudioContent> audio = dynamic_pointer_cast<AudioContent> (c);
		if (audio) {
			cout << "\t" << audio->audio_delay() << " delay\n"
			     << "\t" << audio->audio_gain() << " gain\n";
		}
	}
}

int
main (int argc, char* argv[])
{
	string film_dir;
	bool progress = true;
	bool no_remote = false;
	optional<int> json_port;
	bool keep_going = false;
	bool dump = false;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "flags", no_argument, 0, 'f'},
			{ "no-progress", no_argument, 0, 'n'},
			{ "no-remote", no_argument, 0, 'r'},
			{ "json", required_argument, 0, 'j'},
			{ "keep-going", no_argument, 0, 'k' },
			/* Just using A, B, C ... from here on */
			{ "dump", no_argument, 0, 'A' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "vhfnrj:kA", long_options, &option_index);

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
		case 'j':
			json_port = atoi (optarg);
			break;
		case 'k':
			keep_going = true;
			break;
		case 'A':
			dump = true;
			break;
		}
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
		ServerFinder::instance()->disable ();
	}

	if (json_port) {
		new JSONServer (json_port.get ());
	}

	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir));
		film->read_metadata ();
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
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
			cout << "\033[" << jobs.size() << "A";
			cout.flush ();
		}

		first = false;

		int unfinished = 0;
		int finished_in_error = 0;

		for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
			if (progress) {
				cout << (*i)->name() << ": ";

				if ((*i)->progress ()) {
					cout << (*i)->status() << "			    \n";
				} else {
					cout << ": Running	     \n";
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

	if (keep_going) {
		while (true) {
			dcpomatic_sleep (3600);
		}
	}

	/* This is just to stop valgrind reporting leaks due to JobManager
	   indirectly holding onto codecs.
	*/
	JobManager::drop ();

	ServerFinder::drop ();

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
