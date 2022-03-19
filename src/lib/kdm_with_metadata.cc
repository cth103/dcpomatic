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

#include "i18n.h"


using std::cout;
using std::function;
using std::list;
using std::shared_ptr;
using std::string;
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

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	for (auto i: kdms) {
		auto out = fix_long_path(directory / careful_string_filter(name_format.get(i->name_values(), ".xml")));
		if (!boost::filesystem::exists (out) || confirm_overwrite (out)) {
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

	for (auto const& i: kdms) {
		auto path = directory;
		path /= container_name_format.get(i.front()->name_values(), "", "s");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			boost::filesystem::create_directories (path);
			write_files (i, path, filename_format, confirm_overwrite);
		}
		written += i.size();
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

	for (auto const& i: kdms) {
		auto path = directory;
		path /= container_name_format.get(i.front()->name_values(), ".zip", "s");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			if (boost::filesystem::exists (path)) {
				/* Creating a new zip file over an existing one is an error */
				boost::filesystem::remove (path);
			}
			make_zip_file (i, path, filename_format);
			written += i.size();
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
	string cpl_name
	)
{
	auto config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	for (auto const& i: kdms) {

		if (i.front()->emails().empty()) {
			continue;
		}

		auto zip_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		boost::filesystem::create_directories (zip_file);
		zip_file /= container_name_format.get(i.front()->name_values(), ".zip");
		make_zip_file (i, zip_file, filename_format);

		auto substitute_variables = [cpl_name, i](string target) {
			boost::algorithm::replace_all (target, "$CPL_NAME", cpl_name);
			boost::algorithm::replace_all (target, "$START_TIME", i.front()->get('b').get_value_or(""));
			boost::algorithm::replace_all (target, "$END_TIME", i.front()->get('e').get_value_or(""));
			boost::algorithm::replace_all (target, "$CINEMA_NAME", i.front()->get('c').get_value_or(""));
			return target;
		};

		auto subject = substitute_variables(config->kdm_subject());
		auto body = substitute_variables(config->kdm_email());

		string screens;
		for (auto j: i) {
			auto screen_name = j->get('s');
			if (screen_name) {
				screens += *screen_name + ", ";
			}
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.substr (0, screens.length() - 2));

		Emailer email (config->kdm_from(), i.front()->emails(), subject, body);

		for (auto i: config->kdm_cc()) {
			email.add_cc (i);
		}
		if (!config->kdm_bcc().empty()) {
			email.add_bcc (config->kdm_bcc());
		}

		email.add_attachment (zip_file, container_name_format.get(i.front()->name_values(), ".zip"), "application/zip");

		try {
			email.send (config->mail_server(), config->mail_port(), config->mail_protocol(), config->mail_user(), config->mail_password());
		} catch (...) {
			boost::filesystem::remove (zip_file);
			dcpomatic_log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
			throw;
		}

		boost::filesystem::remove (zip_file);

		dcpomatic_log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
	}
}
