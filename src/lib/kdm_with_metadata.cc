/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "cinema.h"
#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "emailer.h"
#include "kdm_with_metadata.h"
#include "screen.h"
#include "util.h"
#include "zipper.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>

#include "i18n.h"


using std::cout;
using std::function;
using std::list;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


int
write_files (
	list<KDMWithMetadataPtr> kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	std::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (directory == "-") {
		/* Write KDMs to the stdout */
		for (auto i: kdms) {
			cout << i->kdm_as_xml ();
			++written;
		}

		return written;
	}

	if (!dcp::filesystem::exists(directory)) {
		dcp::filesystem::create_directories(directory);
	}

	/* Write KDMs to the specified directory */
	for (auto i: kdms) {
		auto out = directory / careful_string_filter(name_format.get(i->name_values(), ".xml"));
		if (!dcp::filesystem::exists(out) || confirm_overwrite(out)) {
			i->kdm_as_xml (out);
			++written;
		}
	}

	return written;
}


optional<string>
KDMWithMetadata::get (char k) const
{
	auto i = _name_values.find (k);
	if (i == _name_values.end()) {
		return {};
	}

	return i->second;
}


void
make_zip_file (list<KDMWithMetadataPtr> kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format)
{
	Zipper zipper (zip_file);

	for (auto i: kdms) {
		auto const name = careful_string_filter(name_format.get(i->name_values(), ".xml"));
		zipper.add (name, i->kdm_as_xml());
	}

	zipper.close ();
}


/** Collect a list of KDMWithMetadatas into a list of lists so that
 *  each list contains the KDMs for one list.
 */
list<list<KDMWithMetadataPtr>>
collect (list<KDMWithMetadataPtr> kdms)
{
	list<list<KDMWithMetadataPtr>> grouped;

	for (auto i: kdms) {

		auto j = grouped.begin ();

		while (j != grouped.end()) {
			if (j->front()->group() == i->group()) {
				j->push_back (i);
				break;
			}
			++j;
		}

		if (j == grouped.end()) {
			grouped.push_back (list<KDMWithMetadataPtr>());
			grouped.back().push_back (i);
		}
	}

	return grouped;
}


/** Write one directory per list into another directory */
int
write_directories (
	list<list<KDMWithMetadataPtr>> kdms,
	boost::filesystem::path directory,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	for (auto const& kdm: kdms) {
		auto path = directory;
		path /= container_name_format.get(kdm.front()->name_values(), "", "s");
		if (!dcp::filesystem::exists(path) || confirm_overwrite(path)) {
			dcp::filesystem::create_directories(path);
			write_files(kdm, path, filename_format, confirm_overwrite);
			written += kdm.size();
		}
	}

	return written;
}


/** Write one ZIP file per cinema into a directory */
int
write_zip_files (
	list<list<KDMWithMetadataPtr>> kdms,
	boost::filesystem::path directory,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	for (auto const& kdm: kdms) {
		auto path = directory;
		path /= container_name_format.get(kdm.front()->name_values(), ".zip", "s");
		if (!dcp::filesystem::exists(path) || confirm_overwrite(path)) {
			if (dcp::filesystem::exists(path)) {
				/* Creating a new zip file over an existing one is an error */
				dcp::filesystem::remove(path);
			}
			make_zip_file(kdm, path, filename_format);
			written += kdm.size();
		}
	}

	return written;
}


/** Email one ZIP file per cinema to the cinema.
 *  @param kdms KDMs to email.
 *  @param container_name_format Format of folder / ZIP to use.
 *  @param filename_format Format of filenames to use.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 */
void
send_emails (
	list<list<KDMWithMetadataPtr>> kdms,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	string cpl_name,
	vector<string> extra_addresses
	)
{
	auto config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	if (config->kdm_from().empty()) {
		throw NetworkError(_("No KDM from address configured in preferences"));
	}

	for (auto const& kdms_for_cinema: kdms) {

		auto first = kdms_for_cinema.front();

		auto zip_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		dcp::filesystem::create_directories(zip_file);
		zip_file /= container_name_format.get(first->name_values(), ".zip");
		make_zip_file (kdms_for_cinema, zip_file, filename_format);

		auto substitute_variables = [cpl_name, first](string target) {
			boost::algorithm::replace_all(target, "$CPL_NAME", cpl_name);
			boost::algorithm::replace_all(target, "$START_TIME", first->get('b').get_value_or(""));
			boost::algorithm::replace_all(target, "$END_TIME", first->get('e').get_value_or(""));
			boost::algorithm::replace_all(target, "$CINEMA_NAME", first->get('c').get_value_or(""));
			boost::algorithm::replace_all(target, "$CINEMA_SHORT_NAME", first->get('c').get_value_or("").substr(0, 14));
			return target;
		};

		auto subject = substitute_variables(config->kdm_subject());
		auto body = substitute_variables(config->kdm_email());

		string screens;
		for (auto kdm: kdms_for_cinema) {
			auto screen_name = kdm->get('s');
			if (screen_name) {
				screens += *screen_name + ", ";
			}
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.substr (0, screens.length() - 2));

		auto emails = first->emails();
		std::copy(extra_addresses.begin(), extra_addresses.end(), std::back_inserter(emails));
		if (emails.empty()) {
			continue;
		}

		Emailer email (config->kdm_from(), { emails.front() }, subject, body);

		/* Use CC for the second and subsequent email addresses, so we seem less spammy (#2310) */
		for (auto cc = std::next(emails.begin()); cc != emails.end(); ++cc) {
			email.add_cc(*cc);
		}

		for (auto cc: config->kdm_cc()) {
			email.add_cc (cc);
		}
		if (!config->kdm_bcc().empty()) {
			email.add_bcc (config->kdm_bcc());
		}

		email.add_attachment (zip_file, container_name_format.get(first->name_values(), ".zip"), "application/zip");

		auto log_details = [](Emailer& email) {
			dcpomatic_log->log("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log(email.email(), LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log(email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
		};

		try {
			email.send (config->mail_server(), config->mail_port(), config->mail_protocol(), config->mail_user(), config->mail_password());
		} catch (...) {
			dcp::filesystem::remove(zip_file);
			log_details (email);
			throw;
		}

		log_details (email);

		dcp::filesystem::remove(zip_file);
	}
}
