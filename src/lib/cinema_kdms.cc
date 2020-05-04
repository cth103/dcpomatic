/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "exceptions.h"
#include "cinema_kdms.h"
#include "cinema.h"
#include "screen.h"
#include "config.h"
#include "util.h"
#include "emailer.h"
#include "compose.hpp"
#include "log.h"
#include "zipper.h"
#include "dcpomatic_log.h"
#include "kdm_with_metadata.h"
#include <boost/foreach.hpp>

#include "i18n.h"

using std::list;
using std::cout;
using std::string;
using std::runtime_error;
using boost::shared_ptr;
using boost::function;
using boost::optional;

void
make_zip_file (CinemaKDMs kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values)
{
	Zipper zipper (zip_file);

	name_values['c'] = kdms.cinema->name;

	BOOST_FOREACH (KDMWithMetadataPtr i, kdms.screen_kdms) {
		name_values['i'] = i->kdm_id ();
		string const name = careful_string_filter(name_format.get(name_values, ".xml"));
		zipper.add (name, i->kdm_as_xml());
	}

	zipper.close ();
}

/** Collect a list of KDMWithMetadatas into a list of CinemaKDMs so that each
 *  CinemaKDM contains the KDMs for its cinema.
 */
list<CinemaKDMs>
collect (list<KDMWithMetadataPtr> screen_kdms)
{
	list<CinemaKDMs> cinema_kdms;

	while (!screen_kdms.empty ()) {

		/* Get all the screens from a single cinema */

		CinemaKDMs ck;

		list<KDMWithMetadataPtr>::iterator i = screen_kdms.begin ();
		ck.cinema = (*i)->cinema();
		ck.screen_kdms.push_back (*i);
		list<KDMWithMetadataPtr>::iterator j = i;
		++i;
		screen_kdms.remove (*j);

		while (i != screen_kdms.end ()) {
			if ((*i)->cinema() == ck.cinema) {
				ck.screen_kdms.push_back (*i);
				list<KDMWithMetadataPtr>::iterator j = i;
				++i;
				screen_kdms.remove (*j);
			} else {
				++i;
			}
		}

		cinema_kdms.push_back (ck);
	}

	return cinema_kdms;
}

/** Write one directory per cinema into another directory */
int
write_directories (
	list<CinemaKDMs> cinema_kdms,
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

	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		name_values['c'] = i.cinema->name;
		path /= container_name_format.get(name_values, "");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			boost::filesystem::create_directories (path);
			write_files (i.screen_kdms, path, filename_format, name_values, confirm_overwrite);
		}
		written += i.screen_kdms.size();
	}

	return written;
}

/** Write one ZIP file per cinema into a directory */
int
write_zip_files (
	list<CinemaKDMs> cinema_kdms,
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

	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		name_values['c'] = i.cinema->name;
		path /= container_name_format.get(name_values, ".zip");
		if (!boost::filesystem::exists (path) || confirm_overwrite (path)) {
			if (boost::filesystem::exists (path)) {
				/* Creating a new zip file over an existing one is an error */
				boost::filesystem::remove (path);
			}
			make_zip_file (i, path, filename_format, name_values);
			written += i.screen_kdms.size();
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
	list<CinemaKDMs> cinema_kdms,
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

	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {

		if (i.cinema->emails.empty()) {
			continue;
		}

		name_values['c'] = i.cinema->name;

		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		boost::filesystem::create_directories (zip_file);
		zip_file /= container_name_format.get(name_values, ".zip");
		make_zip_file (i, zip_file, filename_format, name_values);

		string subject = config->kdm_subject();
		boost::algorithm::replace_all (subject, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (subject, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (subject, "$END_TIME", name_values['e']);
		boost::algorithm::replace_all (subject, "$CINEMA_NAME", i.cinema->name);

		string body = config->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (body, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (body, "$END_TIME", name_values['e']);
		boost::algorithm::replace_all (body, "$CINEMA_NAME", i.cinema->name);

		string screens;
		BOOST_FOREACH (KDMWithMetadataPtr j, i.screen_kdms) {
			optional<string> screen_name = j->get('n');
			if (screen_name) {
				screens += *screen_name + ", ";
			}
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.substr (0, screens.length() - 2));

		Emailer email (config->kdm_from(), i.cinema->emails, subject, body);

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
