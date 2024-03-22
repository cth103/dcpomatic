/*
    Copyright (C) 2013-2022 Carl Hetherington <cth@carlh.net>

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


#include "cinema.h"
#include "config.h"
#include "dkdm_wrapper.h"
#include "email.h"
#include "exceptions.h"
#include "film.h"
#include "kdm_with_metadata.h"
#include "screen.h"
#include <dcp/certificate.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/encrypted_kdm.h>
#include <dcp/filesystem.h>
#include <getopt.h>


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static void
help (std::function<void (string)> out)
{
	out (String::compose("Syntax: %1 [OPTION] [COMMAND] <FILM|CPL-ID|DKDM>", program_name));
	out ("Commands:");
	out ("create          create KDMs; default if no other command is specified");
	out ("list-cinemas    list known cinemas from DCP-o-matic settings");
	out ("list-dkdm-cpls  list CPLs for which DCP-o-matic has DKDMs");
	out ("  -h, --help                               show this help");
	out ("  -o, --output <path>                      output file or directory");
	out ("  -K, --filename-format <format>           filename format for KDMs");
	out ("  -Z, --container-name-format <format>     filename format for ZIP containers");
	out ("  -f, --valid-from <time>                  valid from time (e.g. \"2013-09-28T01:41:51+04:00\", \"2018-01-01T12:00:30\") or \"now\"");
	out ("  -t, --valid-to <time>                    valid to time (e.g. \"2014-09-28T01:41:51\")");
	out ("  -d, --valid-duration <duration>          valid duration (e.g. \"1 day\", \"4 hours\", \"2 weeks\")");
	out ("  -F, --formulation <formulation>          modified-transitional-1, multiple-modified-transitional-1, dci-any or dci-specific [default modified-transitional-1]");
	out ("  -p, --disable-forensic-marking-picture   disable forensic marking of pictures essences");
	out ("  -a, --disable-forensic-marking-audio     disable forensic marking of audio essences (optionally above a given channel, e.g 12)");
	out ("  -e, --email                              email KDMs to cinemas");
	out ("  -z, --zip                                ZIP each cinema's KDMs into its own file");
	out ("  -v, --verbose                            be verbose");
	out ("  -c, --cinema <name|email>                cinema name (when using -C) or name/email (to filter cinemas)");
	out ("  -S, --screen <name>                      screen name (when using -C) or screen name (to filter screens when using -c)");
	out ("  -C, --projector-certificate <file>       file containing projector certificate");
	out ("  -T, --trusted-device-certificate <file>  file containing a trusted device's certificate");
	out ("      --decryption-key <file>              file containing the private key which can decrypt the given DKDM");
	out ("                                           (DCP-o-matic's configured private key will be used otherwise)");
	out ("      --cinemas-file <file>                use the given file as a list of cinemas instead of the current configuration");
	out ("");
	out ("CPL-ID must be the ID of a CPL that is mentioned in DCP-o-matic's DKDM list.");
	out ("");
	out ("For example:");
	out ("");
	out ("Create KDMs for my_great_movie to play in all of Fred's Cinema's screens for the next two weeks and zip them up.");
	out ("(Fred's Cinema must have been set up in DCP-o-matic's KDM window)");
	out ("");
	out (String::compose("\t%1 -c \"Fred's Cinema\" -f now -d \"2 weeks\" -z my_great_movie", program_name));
}


class KDMCLIError : public std::runtime_error
{
public:
	KDMCLIError (std::string message)
		: std::runtime_error (String::compose("%1: %2", program_name, message).c_str())
	{}
};


static boost::posix_time::time_duration
duration_from_string (string d)
{
	int N;
	char unit_buf[64] = "\0";
	sscanf (d.c_str(), "%d %63s", &N, unit_buf);
	string const unit (unit_buf);

	if (N == 0) {
		throw KDMCLIError (String::compose("could not understand duration \"%1\"", d));
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

	throw KDMCLIError (String::compose("could not understand duration \"%1\"", d));
}


static bool
always_overwrite ()
{
	return true;
}


static
void
write_files (
	list<KDMWithMetadataPtr> kdms,
	bool zip,
	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	bool verbose,
	std::function<void (string)> out
	)
{
	if (zip) {
		int const N = write_zip_files (
			collect (kdms),
			output,
			container_name_format,
			filename_format,
			bind (&always_overwrite)
			);

		if (verbose) {
			out (String::compose("Wrote %1 ZIP files to %2", N, output));
		}
	} else {
		int const N = write_files (
			kdms, output, filename_format,
			bind (&always_overwrite)
			);

		if (verbose) {
			out (String::compose("Wrote %1 KDM files to %2", N, output));
		}
	}
}


static
shared_ptr<Cinema>
find_cinema (string cinema_name)
{
	auto cinemas = Config::instance()->cinemas ();
	auto i = cinemas.begin();
	while (
		i != cinemas.end() &&
		(*i)->name != cinema_name &&
		find ((*i)->emails.begin(), (*i)->emails.end(), cinema_name) == (*i)->emails.end()) {

		++i;
	}

	if (i == cinemas.end ()) {
		throw KDMCLIError (String::compose("could not find cinema \"%1\"", cinema_name));
	}

	return *i;
}


static
void
from_film (
	vector<shared_ptr<Screen>> screens,
	boost::filesystem::path film_dir,
	bool verbose,
	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::LocalTime valid_from,
	dcp::LocalTime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio,
	bool email,
	bool zip,
	std::function<void (string)> out
	)
{
	shared_ptr<Film> film;
	try {
		film = make_shared<Film>(film_dir);
		film->read_metadata ();
		if (verbose) {
			out (String::compose("Read film %1", film->name()));
		}
	} catch (std::exception& e) {
		throw KDMCLIError (String::compose("error reading film \"%1\" (%2)", film_dir.string(), e.what()));
	}

	/* XXX: allow specification of this */
	vector<CPLSummary> cpls = film->cpls ();
	if (cpls.empty ()) {
		throw KDMCLIError ("no CPLs found in film");
	} else if (cpls.size() > 1) {
		throw KDMCLIError ("more than one CPL found in film");
	}

	auto cpl = cpls.front().cpl_file;

	std::vector<KDMCertificatePeriod> period_checks;

	try {
		list<KDMWithMetadataPtr> kdms;
		for (auto i: screens) {
			std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [film, cpl](dcp::LocalTime begin, dcp::LocalTime end) {
				return film->make_kdm(cpl, begin, end);
			};
			auto p = kdm_for_screen(make_kdm, i, valid_from, valid_to, formulation, disable_forensic_marking_picture, disable_forensic_marking_audio, period_checks);
			if (p) {
				kdms.push_back (p);
			}
		}

		if (find_if(
			period_checks.begin(),
			period_checks.end(),
			[](KDMCertificatePeriod const& p) { return p.overlap == KDMCertificateOverlap::KDM_OUTSIDE_CERTIFICATE; }
		   ) != period_checks.end()) {
			throw KDMCLIError(
				"Some KDMs would have validity periods which are completely outside the recipient certificate periods.  Such KDMs are very unlikely to work, so will not be created."
				);
		}

		if (find_if(
			period_checks.begin(),
			period_checks.end(),
			[](KDMCertificatePeriod const& p) { return p.overlap == KDMCertificateOverlap::KDM_OVERLAPS_CERTIFICATE; }
		   ) != period_checks.end()) {
			out("For some of these KDMs the recipient certificate's validity period will not cover the whole of the KDM validity period.  This might cause problems with the KDMs.");
		}

		write_files (kdms, zip, output, container_name_format, filename_format, verbose, out);
		if (email) {
			send_emails ({kdms}, container_name_format, filename_format, film->dcp_name(), {});
		}
	} catch (FileError& e) {
		throw KDMCLIError (String::compose("%1 (%2)", e.what(), e.file().string()));
	}
}


static
optional<dcp::EncryptedKDM>
sub_find_dkdm (shared_ptr<DKDMGroup> group, string cpl_id)
{
	for (auto i: group->children()) {
		auto g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			auto dkdm = sub_find_dkdm (g, cpl_id);
			if (dkdm) {
				return dkdm;
			}
		} else {
			auto d = dynamic_pointer_cast<DKDM>(i);
			assert (d);
			if (d->dkdm().cpl_id() == cpl_id) {
				return d->dkdm();
			}
		}
	}

	return {};
}


static
optional<dcp::EncryptedKDM>
find_dkdm (string cpl_id)
{
	return sub_find_dkdm (Config::instance()->dkdms(), cpl_id);
}


static
dcp::EncryptedKDM
kdm_from_dkdm (
	dcp::DecryptedKDM dkdm,
	dcp::Certificate target,
	vector<string> trusted_devices,
	dcp::LocalTime valid_from,
	dcp::LocalTime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio
	)
{
	/* Signer for new KDM */
	auto signer = Config::instance()->signer_chain ();
	if (!signer->valid ()) {
		throw KDMCLIError ("signing certificate chain is invalid.");
	}

	/* Make a new empty KDM and add the keys from the DKDM to it */
	dcp::DecryptedKDM kdm (
		valid_from,
		valid_to,
		dkdm.annotation_text().get_value_or(""),
		dkdm.content_title_text(),
		dcp::LocalTime().as_string()
		);

	for (auto const& j: dkdm.keys()) {
		kdm.add_key(j);
	}

	return kdm.encrypt (signer, target, trusted_devices, formulation, disable_forensic_marking_picture, disable_forensic_marking_audio);
}


static
void
from_dkdm (
	vector<shared_ptr<Screen>> screens,
	dcp::DecryptedKDM dkdm,
	bool verbose,
 	boost::filesystem::path output,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::LocalTime valid_from,
	dcp::LocalTime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio,
	bool email,
	bool zip,
	std::function<void (string)> out
	)
{
	dcp::NameFormat::Map values;

	try {
		list<KDMWithMetadataPtr> kdms;
		for (auto i: screens) {
			if (!i->recipient) {
				continue;
			}

			auto const kdm = kdm_from_dkdm(
							dkdm,
							i->recipient.get(),
							i->trusted_device_thumbprints(),
							valid_from,
							valid_to,
							formulation,
							disable_forensic_marking_picture,
							disable_forensic_marking_audio
							);

			dcp::NameFormat::Map name_values;
			name_values['c'] = i->cinema ? i->cinema->name : "";
			name_values['s'] = i->name;
			name_values['f'] = kdm.content_title_text();
			name_values['b'] = valid_from.date() + " " + valid_from.time_of_day(true, false);
			name_values['e'] = valid_to.date() + " " + valid_to.time_of_day(true, false);
			name_values['i'] = kdm.cpl_id();

			kdms.push_back(make_shared<KDMWithMetadata>(name_values, i->cinema.get(), i->cinema ? i->cinema->emails : vector<string>(), kdm));
		}
		write_files (kdms, zip, output, container_name_format, filename_format, verbose, out);
		if (email) {
			send_emails ({kdms}, container_name_format, filename_format, dkdm.annotation_text().get_value_or(""), {});
		}
	} catch (FileError& e) {
		throw KDMCLIError (String::compose("%1 (%2)", e.what(), e.file().string()));
	}
}


static
void
dump_dkdm_group (shared_ptr<DKDMGroup> group, int indent, std::function<void (string)> out)
{
	auto const indent_string = string(indent, ' ');

	if (indent > 0) {
		out (indent_string + group->name());
	}
	for (auto i: group->children()) {
		auto g = dynamic_pointer_cast<DKDMGroup>(i);
		if (g) {
			dump_dkdm_group (g, indent + 2, out);
		} else {
			auto d = dynamic_pointer_cast<DKDM>(i);
			assert(d);
			out (indent_string + d->dkdm().cpl_id());
		}
	}
}


static
dcp::LocalTime
time_from_string(string time)
{
	if (time == "now") {
		return {};
	}

	if (time.length() > 10 && time[10] == ' ') {
		time[10] = 'T';
	}

	return dcp::LocalTime(time);
}


optional<string>
kdm_cli (int argc, char* argv[], std::function<void (string)> out)
try
{
	boost::filesystem::path output = dcp::filesystem::current_path();
	auto container_name_format = Config::instance()->kdm_container_name_format();
	auto filename_format = Config::instance()->kdm_filename_format();
	optional<string> cinema_name;
	shared_ptr<Cinema> cinema;
	optional<boost::filesystem::path> projector_certificate;
	optional<boost::filesystem::path> decryption_key;
	optional<string> screen;
	vector<shared_ptr<Screen>> screens;
	optional<dcp::EncryptedKDM> dkdm;
	optional<dcp::LocalTime> valid_from;
	optional<dcp::LocalTime> valid_to;
	bool zip = false;
	string command = "create";
	optional<string> duration_string;
	bool verbose = false;
	dcp::Formulation formulation = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
	bool disable_forensic_marking_picture = false;
	optional<int> disable_forensic_marking_audio;
	bool email = false;
	optional<boost::filesystem::path> cinemas_file;

	program_name = argv[0];

	/* Reset getopt() so we can call this method several times in one test process */
	optind = 1;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h'},
			{ "output", required_argument, 0, 'o'},
			{ "filename-format", required_argument, 0, 'K'},
			{ "container-name-format", required_argument, 0, 'Z'},
			{ "valid-from", required_argument, 0, 'f'},
			{ "valid-to", required_argument, 0, 't'},
			{ "valid-duration", required_argument, 0, 'd'},
			{ "formulation", required_argument, 0, 'F' },
			{ "disable-forensic-marking-picture", no_argument, 0, 'p' },
			{ "disable-forensic-marking-audio", optional_argument, 0, 'a' },
			{ "email", no_argument, 0, 'e' },
			{ "zip", no_argument, 0, 'z' },
			{ "verbose", no_argument, 0, 'v' },
			{ "cinema", required_argument, 0, 'c' },
			{ "screen", required_argument, 0, 'S' },
			{ "projector-certificate", required_argument, 0, 'C' },
			{ "trusted-device-certificate", required_argument, 0, 'T' },
			{ "decryption-key", required_argument, 0, 'G' },
			{ "cinemas-file", required_argument, 0, 'E' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long (argc, argv, "ho:K:Z:f:t:d:F:pae::zvc:S:C:T:E:G:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (out);
			return {};
		case 'o':
			output = optarg;
			break;
		case 'K':
			filename_format = dcp::NameFormat (optarg);
			break;
		case 'Z':
			container_name_format = dcp::NameFormat (optarg);
			break;
		case 'f':
			valid_from = time_from_string(optarg);
			break;
		case 't':
			valid_to = dcp::LocalTime(optarg);
			break;
		case 'd':
			duration_string = optarg;
			break;
		case 'F':
			if (string(optarg) == "modified-transitional-1") {
				formulation = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
			} else if (string(optarg) == "multiple-modified-transitional-1") {
				formulation = dcp::Formulation::MULTIPLE_MODIFIED_TRANSITIONAL_1;
			} else if (string(optarg) == "dci-any") {
				formulation = dcp::Formulation::DCI_ANY;
			} else if (string(optarg) == "dci-specific") {
				formulation = dcp::Formulation::DCI_SPECIFIC;
			} else {
				throw KDMCLIError ("unrecognised KDM formulation " + string (optarg));
			}
			break;
		case 'p':
			disable_forensic_marking_picture = true;
			break;
		case 'a':
			disable_forensic_marking_audio = 0;
			if (optarg == 0 && argv[optind] != 0 && argv[optind][0] != '-') {
				disable_forensic_marking_audio = atoi (argv[optind++]);
			} else if (optarg) {
				disable_forensic_marking_audio = atoi (optarg);
			}
			break;
		case 'e':
			email = true;
			break;
		case 'z':
			zip = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'c':
			/* This could be a cinema to search for in the configured list or the name of a cinema being
			   built up on-the-fly in the option.  Cater for both possilibities here by storing the name
			   (for lookup) and by creating a Cinema which the next Screen will be added to.
			*/
			cinema_name = optarg;
			cinema = make_shared<Cinema>(optarg, vector<string>(), "");
			break;
		case 'S':
			/* Similarly, this could be the name of a new (temporary) screen or the name of a screen
			 * to search for.
			 */
			screen = optarg;
			break;
		case 'C':
			projector_certificate = optarg;
			break;
		case 'T':
			/* A trusted device ends up in the last screen we made */
			if (!screens.empty ()) {
				screens.back()->trusted_devices.push_back(TrustedDevice(dcp::Certificate(dcp::file_to_string(optarg))));
			}
			break;
		case 'G':
			decryption_key = optarg;
			break;
		case 'E':
			cinemas_file = optarg;
			break;
		}
	}

	vector<string> commands = {
		"create",
		"list-cinemas",
		"list-dkdm-cpls"
	};

	if (optind < argc - 1) {
		/* Command with some KDM / CPL / whever specified afterwards */
		command = argv[optind++];
	} else if (optind < argc) {
		/* Look for a valid command, hoping that it's not the name of the KDM / CPL / whatever */
		if (std::find(commands.begin(), commands.end(), argv[optind]) != commands.end()) {
			command = argv[optind];
		}
	}

	if (std::find(commands.begin(), commands.end(), command) == commands.end()) {
		throw KDMCLIError(String::compose("Unrecognised command %1", command));
	}

	if (cinemas_file) {
		Config::instance()->set_cinemas_file(*cinemas_file);
	}

	if (projector_certificate) {
		/* Make a new screen and add it to the current cinema */
		dcp::CertificateChain chain(dcp::file_to_string(*projector_certificate));
		auto screen_to_add = std::make_shared<Screen>(screen.get_value_or(""), "", chain.leaf(), boost::none, vector<TrustedDevice>());
		if (cinema) {
			cinema->add_screen(screen_to_add);
		}
		screens.push_back(screen_to_add);
	}

	if (command == "list-cinemas") {
		auto cinemas = Config::instance()->cinemas ();
		for (auto i: cinemas) {
			out (String::compose("%1 (%2)", i->name, Email::address_list(i->emails)));
		}
		return {};
	}

	if (command == "list-dkdm-cpls") {
		dump_dkdm_group (Config::instance()->dkdms(), 0, out);
		return {};
	}

	if (!duration_string && !valid_to) {
		throw KDMCLIError ("you must specify a --valid-duration or --valid-to");
	}

	if (!valid_from) {
		throw KDMCLIError ("you must specify --valid-from");
	}

	if (optind >= argc) {
		throw KDMCLIError ("no film, CPL ID or DKDM specified");
	}

	if (screens.empty()) {
		if (!cinema_name) {
			throw KDMCLIError ("you must specify either a cinema or one or more screens using certificate files");
		}

		screens = find_cinema (*cinema_name)->screens ();
		if (screen) {
			screens.erase(std::remove_if(screens.begin(), screens.end(), [&screen](shared_ptr<Screen> s) { return s->name != *screen; }), screens.end());
		}
	}

	if (duration_string) {
		valid_to = valid_from.get();
		valid_to->add(duration_from_string(*duration_string));
	}

	if (verbose) {
		out(String::compose("Making KDMs valid from %1 to %2", valid_from->as_string(), valid_to->as_string()));
	}

	string const thing = argv[optind];
	if (dcp::filesystem::is_directory(thing) && dcp::filesystem::is_regular_file(boost::filesystem::path(thing) / "metadata.xml")) {
		from_film (
			screens,
			thing,
			verbose,
			output,
			container_name_format,
			filename_format,
			*valid_from,
			*valid_to,
			formulation,
			disable_forensic_marking_picture,
			disable_forensic_marking_audio,
			email,
			zip,
			out
			);
	} else {
		if (dcp::filesystem::is_regular_file(thing)) {
			dkdm = dcp::EncryptedKDM (dcp::file_to_string (thing));
		} else {
			dkdm = find_dkdm (thing);
		}

		if (!dkdm) {
			throw KDMCLIError ("could not find film or CPL ID corresponding to " + thing);
		}

		string const key = decryption_key ? dcp::file_to_string(*decryption_key) : Config::instance()->decryption_chain()->key().get();

		from_dkdm (
			screens,
			dcp::DecryptedKDM(*dkdm, key),
			verbose,
			output,
			container_name_format,
			filename_format,
			*valid_from,
			*valid_to,
			formulation,
			disable_forensic_marking_picture,
			disable_forensic_marking_audio,
			email,
			zip,
			out
			);
	}

	return {};
} catch (std::exception& e) {
	return string(e.what());
}

