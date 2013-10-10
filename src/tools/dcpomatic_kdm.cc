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

#include <getopt.h>
#include <libdcp/certificates.h>
#include "lib/film.h"
#include "lib/cinema.h"
#include "lib/kdm.h"
#include "lib/config.h"
#include "lib/exceptions.h"

using std::string;
using std::cout;
using std::cerr;
using std::list;
using boost::shared_ptr;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] [<FILM>]\n"
	     << "  -h, --help        show this help\n"
	     << "  -o, --output      output file or directory\n"
	     << "  -f, --valid-from  valid from time (e.g. \"2013-09-28 01:41:51\")\n"
	     << "  -t, --valid-to    valid to time (e.g. \"2014-09-28 01:41:51\")\n"
	     << "  -c, --certificate file containing projector certificate\n"
	     << "  -z, --zip         ZIP each cinema's KDMs into its own file\n"
	     << "      --cinema      specify a cinema, either by name or email address\n"
	     << "      --cinemas     list known cinemas from the DCP-o-matic settings\n";
}

int main (int argc, char* argv[])
{
	boost::filesystem::path output;
	boost::posix_time::ptime valid_from;
	boost::posix_time::ptime valid_to;
	string certificate_file;
	bool zip = false;
	string cinema_name;
	bool cinemas = false;
	
	int option_index = 0;
	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
			{ "valid-from", required_argument, 0, 'f'},
			{ "valid-to", required_argument, 0, 't'},
			{ "certificate", required_argument, 0, 'c' },
			{ "cinema", required_argument, 0, 'A' },
			{ "cinemas", no_argument, 0, 'B' },
			{ "zip", no_argument, 0, 'z' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:f:t:c:A:Bz", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'o':
			output = optarg;
			break;
		case 'f':
			valid_from = boost::posix_time::time_from_string (optarg);
			break;
		case 't':
			valid_to = boost::posix_time::time_from_string (optarg);
			break;
		case 'c':
			certificate_file = optarg;
			break;
		case 'A':
			cinema_name = optarg;
			break;
		case 'B':
			cinemas = true;
			break;
		case 'z':
			zip = true;
			break;
		}
	}

	if (cinemas) {
		list<boost::shared_ptr<Cinema> > cinemas = Config::instance()->cinemas ();
		for (list<boost::shared_ptr<Cinema> >::const_iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
			cout << (*i)->name << " (" << (*i)->email << ")\n";
		}
		exit (EXIT_SUCCESS);
	}

	if (optind >= argc) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	if (cinema_name.empty() && certificate_file.empty()) {
		cerr << argv[0] << ": you must specify either a cinema, a screen or a certificate file\n";
		exit (EXIT_FAILURE);
	}

	string const film_dir = argv[optind];
			
	dcpomatic_setup ();

	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir));
		film->read_metadata ();
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	if (cinema_name.empty ()) {
		shared_ptr<libdcp::Certificate> certificate (new libdcp::Certificate (boost::filesystem::path (certificate_file)));
		libdcp::KDM kdm = film->make_kdm (certificate, valid_from, valid_to);
		kdm.as_xml (output);
	} else {

		list<shared_ptr<Cinema> > cinemas = Config::instance()->cinemas ();
		list<shared_ptr<Cinema> >::const_iterator i = cinemas.begin();
		while (i != cinemas.end() && (*i)->name != cinema_name && (*i)->email != cinema_name) {
			++i;
		}

		if (i == cinemas.end ()) {
			cerr << argv[0] << ": could not find cinema \"" << cinema_name << "\"\n";
			exit (EXIT_FAILURE);
		}

		try {
			if (zip) {
				write_kdm_zip_files (film, (*i)->screens(), valid_from, valid_to, output);
			} else {
				write_kdm_files (film, (*i)->screens(), valid_from, valid_to, output);
			}
		} catch (FileError& e) {
			cerr << argv[0] << ": " << e.what() << " (" << e.file().string() << ")\n";
			exit (EXIT_FAILURE);
		}
	}

	return 0;
}
