/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "kdm_with_metadata.h"
#include "cinema.h"
#include "screen.h"
#include "util.h"
#include "zipper.h"
#include "config.h"
#include "dcpomatic_log.h"
#include "emailer.h"
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/function.hpp>

#include "i18n.h"

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::function;

int
write_files (
	list<KDMWithMetadataPtr> kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (directory == "-") {
		/* Write KDMs to the stdout */
		BOOST_FOREACH (KDMWithMetadataPtr i, kdms) {
			cout << i->kdm_as_xml ();
			++written;
		}

		return written;
	}

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	BOOST_FOREACH (KDMWithMetadataPtr i, kdms) {
		name_values['i'] = i->kdm_id ();
		boost::filesystem::path out = directory / careful_string_filter(name_format.get(name_values, ".xml"));
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
	dcp::NameFormat::Map::const_iterator i = _name_values.find (k);
	if (i == _name_values.end()) {
		return optional<string>();
	}

	return i->second;
}


void
make_zip_file (list<KDMWithMetadataPtr> kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values)
{
	Zipper zipper (zip_file);

	BOOST_FOREACH (KDMWithMetadataPtr i, kdms) {
		name_values['i'] = i->kdm_id ();
		string const name = careful_string_filter(name_format.get(name_values, ".xml"));
		zipper.add (name, i->kdm_as_xml());
	}

	zipper.close ();
}


/** Collect a list of KDMWithMetadatas into a list of lists so that
 *  each list contains the KDMs for one cinema.
 */
list<list<KDMWithMetadataPtr> >
collect (list<KDMWithMetadataPtr> kdms)
{
	list<list<KDMWithMetadataPtr> > grouped;

	BOOST_FOREACH (KDMWithMetadataPtr i, kdms) {

		list<list<KDMWithMetadataPtr> >::iterator j = grouped.begin ();

		while (j != grouped.end()) {
			if (j->front()->cinema() == i->cinema()) {
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


/** Write one directory per cinema into another directory */
int
write_directories (
	list<list<KDMWithMetadataPtr> > cinema_kdms,
	boost::filesystem::path directory,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	/* No specific screen */
	name_values['s'] = "";

	int written = 0;

	BOOST_FOREACH (list<KDMWithMetadataPtr> const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		path /= container_name_format.get(name_values, "");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			boost::filesystem::create_directories (path);
			write_files (i, path, filename_format, name_values, confirm_overwrite);
		}
		written += i.size();
	}

	return written;
}


/** Write one ZIP file per cinema into a directory */
int
write_zip_files (
	list<list<KDMWithMetadataPtr> > cinema_kdms,
	boost::filesystem::path directory,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	/* No specific screen */
	name_values['s'] = "";

	int written = 0;

	BOOST_FOREACH (list<KDMWithMetadataPtr> const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		path /= container_name_format.get(name_values, ".zip");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			if (boost::filesystem::exists (path)) {
				/* Creating a new zip file over an existing one is an error */
				boost::filesystem::remove (path);
			}
			make_zip_file (i, path, filename_format, name_values);
			written += i.size();
		}
	}

	return written;
}


/** Email one ZIP file per cinema to the cinema.
 *  @param cinema_kdms KDMS to email.
 *  @param container_name_format Format of folder / ZIP to use.
 *  @param filename_format Format of filenames to use.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 */
void
email (
	list<list<KDMWithMetadataPtr> > cinema_kdms,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	string cpl_name
	)
{
	Config* config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	/* No specific screen */
	name_values['s'] = "";

	BOOST_FOREACH (list<KDMWithMetadataPtr> const & i, cinema_kdms) {

		if (i.front()->cinema()->emails.empty()) {
			continue;
		}

		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		boost::filesystem::create_directories (zip_file);
		zip_file /= container_name_format.get(name_values, ".zip");
		make_zip_file (i, zip_file, filename_format, name_values);

		string subject = config->kdm_subject();
		boost::algorithm::replace_all (subject, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (subject, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (subject, "$END_TIME", name_values['e']);
		boost::algorithm::replace_all (subject, "$CINEMA_NAME", i.front()->cinema()->name);

		string body = config->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (body, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (body, "$END_TIME", name_values['e']);
		boost::algorithm::replace_all (body, "$CINEMA_NAME", i.front()->cinema()->name);

		string screens;
		BOOST_FOREACH (KDMWithMetadataPtr j, i) {
			optional<string> screen_name = j->get('n');
			if (screen_name) {
				screens += *screen_name + ", ";
			}
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.substr (0, screens.length() - 2));

		Emailer email (config->kdm_from(), i.front()->cinema()->emails, subject, body);

		BOOST_FOREACH (string i, config->kdm_cc()) {
			email.add_cc (i);
		}
		if (!config->kdm_bcc().empty ()) {
			email.add_bcc (config->kdm_bcc ());
		}

		email.add_attachment (zip_file, container_name_format.get(name_values, ".zip"), "application/zip");

		Config* c = Config::instance ();

		try {
			email.send (c->mail_server(), c->mail_port(), c->mail_protocol(), c->mail_user(), c->mail_password());
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
