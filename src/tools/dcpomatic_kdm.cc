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

using std::string;
using std::cerr;
using boost::shared_ptr;

static void
help (string n)
{
	cerr << "Syntax: " << n << " [OPTION] <FILM>\n"
	     << "  -h, --help        show this help\n"
	     << "  -o, --output      output filename\n"
	     << "  -f, --valid-from  valid from time (e.g. \"2013-09-28 01:41:51\")\n"
	     << "  -t, --valid-to    valid to time (e.g. \"2014-09-28 01:41:51\")\n"
	     << "  -c, --certificate file containing projector certificate\n";
}

int main (int argc, char* argv[])
{
	string output;
	string valid_from;
	string valid_to;
	string certificate_file;
	
	int option_index = 0;
	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
			{ "valid-from", required_argument, 0, 'f'},
			{ "valid-to", required_argument, 0, 't'},
			{ "certificate", required_argument, 0, 'c' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:f:t:c:", long_options, &option_index);

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
			valid_from = optarg;
			break;
		case 't':
			valid_to = optarg;
			break;
		case 'c':
			certificate_file = optarg;
			break;
		}
	}

	if (optind >= argc) {
		help (argv[0]);
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

	cerr << "reading " << certificate_file << "\n";
	shared_ptr<libdcp::Certificate> certificate (new libdcp::Certificate (boost::filesystem::path (certificate_file)));
	libdcp::KDM kdm = film->make_kdm (
		certificate, boost::posix_time::time_from_string (valid_from), boost::posix_time::time_from_string (valid_to)
		);

	kdm.as_xml (output);
	return 0;
}
