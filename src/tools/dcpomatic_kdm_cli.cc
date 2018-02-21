/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  src/tools/dcpomatic_kdm_cli.cc
 *  @brief Command-line program to generate KDMs.
 */

#include "lib/film.h"
#include "lib/cinema.h"
#include "lib/screen_kdm.h"
#include "lib/cinema_kdms.h"
#include "lib/config.h"
#include "lib/exceptions.h"
#include "lib/emailer.h"
#include "lib/dkdm_wrapper.h"
#include "lib/screen.h"
#include <dcp/certificate.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/encrypted_kdm.h>
#include <getopt.h>
#include <iostream>

using std::string;
using std::cout;
using std::cerr;
using std::list;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::bind;
using boost::dynamic_pointer_cast;

static void
help ()
{
	cerr << "Syntax: " << program_name << " [OPTION] <FILM|CPL-ID>\n"
		"  -h, --help             show this help\n"
		"  -o, --output           output file or directory\n"
		"  -f, --valid-from       valid from time (in local time zone of the cinema) (e.g. \"2013-09-28 01:41:51\") or \"now\"\n"
		"  -t, --valid-to         valid to time (in local time zone of the cinema) (e.g. \"2014-09-28 01:41:51\")\n"
		"  -d, --valid-duration   valid duration (e.g. \"1 day\", \"4 hours\", \"2 weeks\")\n"
		"      --formulation      modified-transitional-1, dci-any or dci-specific [default modified-transitional-1]\n"
		"  -z, --zip              ZIP each cinema's KDMs into its own file\n"
		"  -v, --verbose          be verbose\n"
		"  -c, --cinema           specify a cinema, either by name or email address\n"
		"      --certificate      file containing projector certificate\n"
		"      --cinemas          list known cinemas from the DCP-o-matic settings\n"
		"      --dkdm-cpls        list CPLs for which DCP-o-matic has DKDMs\n\n"
		"CPL-ID must be the ID of a CPL that is mentioned in DCP-o-matic's DKDM list.\n\n"
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
	int N;
	char unit_buf[64] = "\0";
	sscanf (d.c_str(), "%d %63s", &N, unit_buf);
	string const unit (unit_buf);

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

static bool
always_overwrite ()
{
	return true;
}

void
write_files (list<ScreenKDM> screen_kdms, bool zip, boost::filesystem::path output, dcp::NameFormat::Map values, bool verbose)
{
	if (zip) {
		int const N = CinemaKDMs::write_zip_files (
			CinemaKDMs::collect (screen_kdms),
			output,
			Config::instance()->kdm_container_name_format(),
			Config::instance()->kdm_filename_format(),
			values,
			bind (&always_overwrite)
			);

		if (verbose) {
			cout << "Wrote " << N << " ZIP files to " << output << "\n";
		}
	} else {
		int const N = ScreenKDM::write_files (
			screen_kdms, output, Config::instance()->kdm_filename_format(), values,
			bind (&always_overwrite)
			);

		if (verbose) {
			cout << "Wrote " << N << " KDM files to " << output << "\n";
		}
	}
}

shared_ptr<Cinema>
find_cinema (string cinema_name)
{
	list<shared_ptr<Cinema> > cinemas = Config::instance()->cinemas ();
	list<shared_ptr<Cinema> >::const_iterator i = cinemas.begin();
	while (
		i != cinemas.end() &&
		(*i)->name != cinema_name &&
		find ((*i)->emails.begin(), (*i)->emails.end(), cinema_name) == (*i)->emails.end()) {

		++i;
	}

	if (i == cinemas.end ()) {
		cerr << program_name << ": could not find cinema \"" << cinema_name << "\"\n";
		exit (EXIT_FAILURE);
	}

	return *i;
}

void
from_film (
	boost::filesystem::path film_dir,
	bool verbose,
	optional<string> cinema_name,
	optional<boost::filesystem::path> output,
	optional<boost::filesystem::path> certificate_file,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool zip
	)
{
	shared_ptr<Film> film;
	try {
		film.reset (new Film (film_dir));
		film->read_metadata ();
		if (verbose) {
			cout << "Read film " << film->name () << "\n";
		}
	} catch (std::exception& e) {
		cerr << program_name << ": error reading film `" << film_dir.string() << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	/* XXX: allow specification of this */
	vector<CPLSummary> cpls = film->cpls ();
	if (cpls.empty ()) {
		error ("no CPLs found in film");
	} else if (cpls.size() > 1) {
		error ("more than one CPL found in film");
	}

	boost::filesystem::path cpl = cpls.front().cpl_file;

	if (!cinema_name) {

		if (!output) {
			error ("you must specify --output");
		}

		dcp::Certificate certificate (dcp::file_to_string (*certificate_file));
		dcp::EncryptedKDM kdm = film->make_kdm (
			certificate, vector<dcp::Certificate>(), cpl, dcp::LocalTime (valid_from), dcp::LocalTime (valid_to), formulation
			);
		kdm.as_xml (*output);
		if (verbose) {
			cout << "Generated KDM " << *output << " for certificate.\n";
		}
	} else {

		if (!output) {
			output = ".";
		}

		dcp::NameFormat::Map values;
		values['f'] = film->name();
		values['b'] = dcp::LocalTime(valid_from).date() + " " + dcp::LocalTime(valid_from).time_of_day(true, false);
		values['e'] = dcp::LocalTime(valid_to).date() + " " + dcp::LocalTime(valid_to).time_of_day(true, false);

		try {
			list<ScreenKDM> screen_kdms = film->make_kdms (
				find_cinema(*cinema_name)->screens(), cpl, valid_from, valid_to, formulation
				);

			write_files (screen_kdms, zip, *output, values, verbose);
		} catch (FileError& e) {
			cerr << program_name << ": " << e.what() << " (" << e.file().string() << ")\n";
			exit (EXIT_FAILURE);
		} catch (KDMError& e) {
			cerr << program_name << ": " << e.what() << "\n";
			exit (EXIT_FAILURE);
		}
	}
}

optional<dcp::EncryptedKDM>
sub_find_dkdm (shared_ptr<DKDMGroup> group, string cpl_id)
{
	BOOST_FOREACH (shared_ptr<DKDMBase> i, group->children()) {
		shared_ptr<DKDMGroup> g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			optional<dcp::EncryptedKDM> dkdm = sub_find_dkdm (g, cpl_id);
			if (dkdm) {
				return dkdm;
			}
		} else {
			shared_ptr<DKDM> d = dynamic_pointer_cast<DKDM>(i);
			assert (d);
			if (d->dkdm().cpl_id() == cpl_id) {
				return d->dkdm();
			}
		}
	}

	return optional<dcp::EncryptedKDM>();
}

optional<dcp::EncryptedKDM>
find_dkdm (string cpl_id)
{
	return sub_find_dkdm (Config::instance()->dkdms(), cpl_id);
}

dcp::EncryptedKDM
kdm_from_dkdm (
	dcp::EncryptedKDM dkdm,
	dcp::Certificate target,
	vector<dcp::Certificate> trusted_devices,
	dcp::LocalTime valid_from,
	dcp::LocalTime valid_to,
	dcp::Formulation formulation
	)
{
	/* Decrypted DKDM */
	dcp::DecryptedKDM decrypted_dkdm (dkdm, Config::instance()->decryption_chain()->key().get());

	/* Signer for new KDM */
	shared_ptr<const dcp::CertificateChain> signer = Config::instance()->signer_chain ();
	if (!signer->valid ()) {
		error ("signing certificate chain is invalid.");
	}

	/* Make a new empty KDM and add the keys from the DKDM to it */
	dcp::DecryptedKDM kdm (
		valid_from,
		valid_to,
		dkdm.annotation_text().get_value_or(""),
		dkdm.content_title_text(),
		dcp::LocalTime().as_string()
		);

	BOOST_FOREACH (dcp::DecryptedKDMKey const & j, decrypted_dkdm.keys()) {
		kdm.add_key(j);
	}

	return kdm.encrypt (signer, target, trusted_devices, formulation, true, 0);
}

void
from_dkdm (
	dcp::EncryptedKDM dkdm,
	bool verbose,
	optional<string> cinema_name,
 	optional<boost::filesystem::path> output,
	optional<boost::filesystem::path> certificate_file,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool zip
	)
{
	if (!cinema_name) {
		if (!output) {
			error ("you must specify --output");
		}

		dcp::EncryptedKDM kdm = kdm_from_dkdm (
			dkdm,
			dcp::Certificate (dcp::file_to_string (*certificate_file)),
			vector<dcp::Certificate>(),
			dcp::LocalTime(valid_from), dcp::LocalTime(valid_to),
			formulation
			);

		kdm.as_xml (*output);
		if (verbose) {
			cout << "Generated KDM " << *output << " for certificate.\n";
		}
	} else {

		if (!output) {
			output = ".";
		}

		dcp::NameFormat::Map values;
		values['f'] = dkdm.annotation_text().get_value_or("");
		values['b'] = dcp::LocalTime(valid_from).date() + " " + dcp::LocalTime(valid_from).time_of_day(true, false);
		values['e'] = dcp::LocalTime(valid_to).date() + " " + dcp::LocalTime(valid_to).time_of_day(true, false);

		try {
			list<ScreenKDM> screen_kdms;
			BOOST_FOREACH (shared_ptr<Screen> i, find_cinema(*cinema_name)->screens()) {
				if (!i->recipient) {
					continue;
				}
				screen_kdms.push_back (
					ScreenKDM (
						i,
						kdm_from_dkdm (
							dkdm,
							i->recipient.get(),
							i->trusted_devices,
							dcp::LocalTime(valid_from, i->cinema->utc_offset_hour(), i->cinema->utc_offset_minute()),
							dcp::LocalTime(valid_to, i->cinema->utc_offset_hour(), i->cinema->utc_offset_minute()),
							formulation
							)
						)
					);
			}
			write_files (screen_kdms, zip, *output, values, verbose);
		} catch (FileError& e) {
			cerr << program_name << ": " << e.what() << " (" << e.file().string() << ")\n";
			exit (EXIT_FAILURE);
		} catch (KDMError& e) {
			cerr << program_name << ": " << e.what() << "\n";
			exit (EXIT_FAILURE);
		}
	}
}

void
dump_dkdm_group (shared_ptr<DKDMGroup> group, int indent)
{
	if (indent > 0) {
		for (int i = 0; i < indent; ++i) {
			cout << " ";
		}
		cout << group->name() << "\n";
	}
	BOOST_FOREACH (shared_ptr<DKDMBase> i, group->children()) {
		shared_ptr<DKDMGroup> g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			dump_dkdm_group (g, indent + 2);
		} else {
			for (int j = 0; j < indent; ++j) {
				cout << " ";
			}
			shared_ptr<DKDM> d = dynamic_pointer_cast<DKDM>(i);
			assert(d);
			cout << d->dkdm().cpl_id() << "\n";
		}
	}
}

int main (int argc, char* argv[])
{
	optional<boost::filesystem::path> output;
	optional<boost::posix_time::ptime> valid_from;
	optional<boost::posix_time::ptime> valid_to;
	optional<boost::filesystem::path> certificate_file;
	bool zip = false;
	optional<string> cinema_name;
	bool cinemas = false;
	bool dkdm_cpls = false;
	optional<string> duration_string;
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
			{ "dkdm-cpls", no_argument, 0, 'D' },
			{ "zip", no_argument, 0, 'z' },
			{ "duration", required_argument, 0, 'd' },
			{ "verbose", no_argument, 0, 'v' },
			{ "formulation", required_argument, 0, 'C' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:f:t:c:A:Bzd:vC:D", long_options, &option_index);

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
		case 'D':
			dkdm_cpls = true;
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
			cout << (*i)->name << " (" << Emailer::address_list ((*i)->emails) << ")\n";
		}
		exit (EXIT_SUCCESS);
	}

	if (dkdm_cpls) {
		dump_dkdm_group (Config::instance()->dkdms(), 0);
		exit (EXIT_SUCCESS);
	}

	if (!duration_string && !valid_to) {
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

	if (!cinema_name && !certificate_file) {
		error ("you must specify either a cinema, a screen or a certificate file");
	}

	if (duration_string) {
		valid_to = valid_from.get() + duration_from_string (*duration_string);
	}

	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();

	if (verbose) {
		cout << "Making KDMs valid from " << valid_from.get() << " to " << valid_to.get() << "\n";
	}

	string const thing = argv[optind];
	if (boost::filesystem::is_directory(thing) && boost::filesystem::is_regular_file(boost::filesystem::path(thing) / "metadata.xml")) {
		from_film (thing, verbose, cinema_name, output, certificate_file, *valid_from, *valid_to, formulation, zip);
	} else {
		optional<dcp::EncryptedKDM> dkdm = find_dkdm (thing);
		if (!dkdm) {
			error ("could not find film or CPL ID corresponding to " + thing);
		}
		from_dkdm (*dkdm, verbose, cinema_name, output, certificate_file, *valid_from, *valid_to, formulation, zip);
	}

	return 0;
}
