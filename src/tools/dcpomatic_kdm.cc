/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/tools/dcpomatic_kdm.cc
 *  @brief Command-line program to generate KDMs.
 */

#include <getopt.h>
#include <dcp/certificates.h>
#include "lib/film.h"
#include "lib/cinema.h"
#include "lib/kdm.h"
#include "lib/config.h"
#include "lib/exceptions.h"
#include "lib/safe_stringstream.h"

using std::string;
using std::cout;
using std::cerr;
using std::list;
using std::vector;
using boost::shared_ptr;

static void
help ()
{
	cerr << "Syntax: " << program_name << " [OPTION] [<FILM>]\n"
		"  -h, --help             show this help\n"
		"  -o, --output           output file or directory\n"
		"  -f, --valid-from       valid from time (in local time zone) (e.g. \"2013-09-28 01:41:51\") or \"now\"\n"
		"  -t, --valid-to         valid to time (in local time zone) (e.g. \"2014-09-28 01:41:51\")\n"
		"  -d, --valid-duration   valid duration (e.g. \"1 day\", \"4 hours\", \"2 weeks\")\n"
		"      --formulation      modified-transitional-1, dci-any or dci-specific [default modified-transitional-1]\n"
		"  -z, --zip              ZIP each cinema's KDMs into its own file\n"
		"  -v, --verbose          be verbose\n"
		"  -c, --cinema           specify a cinema, either by name or email address\n"
		"      --cinemas          list known cinemas from the DCP-o-matic settings\n"
		"      --certificate file containing projector certificate\n\n"
		"For example:\n\n"
		"Create KDMs for my_great_movie to play in all of Fred's Cinema's screens for the next two weeks and zip them up.\n"
		"(Fred's Cinema must have been set up in DCP-o-matic's KDM window)\n\n"
		"\tdcpomatic_kdm -c \"Fred's Cinema\" -f now -d \"2 weeks\" -z my_great_movie\n\n";
}

static void
error (string m)
{
	cerr << program_name << ": " << m << "\n";
	exit (EXIT_FAILURE);
}

static boost::posix_time::ptime
time_from_string (string t)
{
	if (t == "now") {
		return boost::posix_time::second_clock::local_time ();
	}

	return boost::posix_time::time_from_string (t);
}

static boost::posix_time::time_duration
duration_from_string (string d)
{
	SafeStringStream s (d);
	int N;
	string unit;
	s >> N >> unit;

	if (N == 0) {
		cerr << "Could not understand duration \"" << d << "\"\n";
		exit (EXIT_FAILURE);
	}

	if (unit == "year" || unit == "years") {
		return boost::posix_time::time_duration (N * 24 * 365, 0, 0, 0);
	} else if (unit == "week" || unit == "weeks") {
		return boost::posix_time::time_duration (N * 24 * 7, 0, 0, 0);
	} else if (unit == "day" || unit == "days") {
		return boost::posix_time::time_duration (N * 24, 0, 0, 0);
	} else if (unit == "hour" || unit == "hours") {
		return boost::posix_time::time_duration (N, 0, 0, 0);
	}

	cerr << "Could not understand duration \"" << d << "\"\n";
	exit (EXIT_FAILURE);
}

int main (int argc, char* argv[])
{
	boost::filesystem::path output;
	boost::optional<boost::posix_time::ptime> valid_from;
	boost::optional<boost::posix_time::ptime> valid_to;
	string certificate_file;
	bool zip = false;
	string cinema_name;
	bool cinemas = false;
	string duration_string;
	bool verbose = false;
	dcp::Formulation formulation = dcp::MODIFIED_TRANSITIONAL_1;

	program_name = argv[0];
	
	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
			{ "valid-from", required_argument, 0, 'f'},
			{ "valid-to", required_argument, 0, 't'},
			{ "certificate", required_argument, 0, 'A' },
			{ "cinema", required_argument, 0, 'c' },
			{ "cinemas", no_argument, 0, 'B' },
			{ "zip", no_argument, 0, 'z' },
			{ "duration", required_argument, 0, 'd' },
			{ "verbose", no_argument, 0, 'v' },
			{ "formulation", required_argument, 0, 'C' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:f:t:c:A:Bzd:vC:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help ();
			exit (EXIT_SUCCESS);
		case 'o':
			output = optarg;
			break;
		case 'f':
			valid_from = time_from_string (optarg);
			break;
		case 't':
			valid_to = time_from_string (optarg);
			break;
		case 'A':
			certificate_file = optarg;
			break;
		case 'c':
			cinema_name = optarg;
			break;
		case 'B':
			cinemas = true;
			break;
		case 'z':
			zip = true;
			break;
		case 'd':
			duration_string = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		case 'C':
			if (string (optarg) == "modified-transitional-1") {
				formulation = dcp::MODIFIED_TRANSITIONAL_1;
			} else if (string (optarg) == "dci-any") {
				formulation = dcp::DCI_ANY;
			} else if (string (optarg) == "dci-specific") {
				formulation = dcp::DCI_SPECIFIC;
			} else {
				error ("unrecognised KDM formulation " + string (optarg));
			}
		}
	}

	if (cinemas) {
		list<boost::shared_ptr<Cinema> > cinemas = Config::instance()->cinemas ();
		for (list<boost::shared_ptr<Cinema> >::const_iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
			cout << (*i)->name << " (" << (*i)->email << ")\n";
		}
		exit (EXIT_SUCCESS);
	}

	if (duration_string.empty() && !valid_to) {
		error ("you must specify a --valid-duration or --valid-to");
	}

	if (!valid_from) {
		error ("you must specify --valid-from");
		exit (EXIT_FAILURE);
	}

	if (optind >= argc) {
		help ();
		exit (EXIT_FAILURE);
	}

	if (cinema_name.empty() && certificate_file.empty()) {
		error ("you must specify either a cinema, a screen or a certificate file");
	}

	if (!duration_string.empty ()) {
		valid_to = valid_from.get() + duration_from_string (duration_string);
	}

	string const film_dir = argv[optind];
			
	dcpomatic_setup ();

	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir));
		film->read_metadata ();
		if (verbose) {
			cout << "Read film " << film->name () << "\n";
		}
	} catch (std::exception& e) {
		cerr << program_name << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	if (verbose) {
		cout << "Making KDMs valid from " << valid_from.get() << " to " << valid_to.get() << "\n";
	}

	/* XXX: allow specification of this */
	vector<CPLSummary> cpls = film->cpls ();
	if (cpls.empty ()) {
		error ("no CPLs found in film");
	} else if (cpls.size() > 1) {
		error ("more than one CPL found in film");
	}

	boost::filesystem::path cpl = cpls.front().cpl_file;

	if (cinema_name.empty ()) {

		if (output.empty ()) {
			error ("you must specify --output");
		}
		
		dcp::Certificate certificate (dcp::file_to_string (certificate_file));
		dcp::EncryptedKDM kdm = film->make_kdm (certificate, cpl, valid_from.get(), valid_to.get(), formulation);
		kdm.as_xml (output);
		if (verbose) {
			cout << "Generated KDM " << output << " for certificate.\n";
		}
	} else {

		list<shared_ptr<Cinema> > cinemas = Config::instance()->cinemas ();
		list<shared_ptr<Cinema> >::const_iterator i = cinemas.begin();
		while (i != cinemas.end() && (*i)->name != cinema_name && (*i)->email != cinema_name) {
			++i;
		}

		if (i == cinemas.end ()) {
			cerr << program_name << ": could not find cinema \"" << cinema_name << "\"\n";
			exit (EXIT_FAILURE);
		}

		if (output.empty ()) {
			output = ".";
		}

		try {
			if (zip) {
				write_kdm_zip_files (
					film, (*i)->screens(), cpl, dcp::LocalTime (valid_from.get()), dcp::LocalTime (valid_to.get()), formulation, output
					);
				
				if (verbose) {
					cout << "Wrote ZIP files to " << output << "\n";
				}
			} else {
				write_kdm_files (
					film, (*i)->screens(), cpl, dcp::LocalTime (valid_from.get()), dcp::LocalTime (valid_to.get()), formulation, output
					);
				
				if (verbose) {
					cout << "Wrote KDM files to " << output << "\n";
				}
			}
		} catch (FileError& e) {
			cerr << argv[0] << ": " << e.what() << " (" << e.file().string() << ")\n";
			exit (EXIT_FAILURE);
		} catch (KDMError& e) {
			cerr << argv[0] << ": " << e.what() << "\n";
			exit (EXIT_FAILURE);
		}
	}

	return 0;
}
