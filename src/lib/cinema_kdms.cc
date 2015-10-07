/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "quickmail.h"
#include "exceptions.h"
#include "cinema_kdms.h"
#include "cinema.h"
#include "screen.h"
#include "config.h"
#include "util.h"
#include "compose.hpp"
#include <zip.h>
#include <boost/foreach.hpp>

using std::list;
using std::string;
using boost::shared_ptr;

/** @param filename_first_part First part of name of KDM files inside the zip file
 *  (perhaps the name of the film).
 */
void
CinemaKDMs::make_zip_file (string filename_first_part, boost::filesystem::path zip_file) const
{
	int error;
	struct zip* zip = zip_open (zip_file.string().c_str(), ZIP_CREATE | ZIP_EXCL, &error);
	if (!zip) {
		if (error == ZIP_ER_EXISTS) {
			throw FileError ("ZIP file already exists", zip_file);
		}
		throw FileError ("could not create ZIP file", zip_file);
	}

	list<shared_ptr<string> > kdm_strings;

	BOOST_FOREACH (ScreenKDM const & i, screen_kdms) {
		shared_ptr<string> kdm (new string (i.kdm.as_xml ()));
		kdm_strings.push_back (kdm);

		struct zip_source* source = zip_source_buffer (zip, kdm->c_str(), kdm->length(), 0);
		if (!source) {
			throw StringError ("could not create ZIP source");
		}

		if (zip_add (zip, i.filename(filename_first_part).c_str(), source) == -1) {
			throw StringError ("failed to add KDM to ZIP archive");
		}
	}

	if (zip_close (zip) == -1) {
		throw StringError ("failed to close ZIP archive");
	}
}

list<CinemaKDMs>
CinemaKDMs::collect (list<ScreenKDM> screen_kdms)
{
	list<CinemaKDMs> cinema_kdms;

	while (!screen_kdms.empty ()) {

		/* Get all the screens from a single cinema */

		CinemaKDMs ck;

		list<ScreenKDM>::iterator i = screen_kdms.begin ();
		ck.cinema = i->screen->cinema;
		ck.screen_kdms.push_back (*i);
		list<ScreenKDM>::iterator j = i;
		++i;
		screen_kdms.remove (*j);

		while (i != screen_kdms.end ()) {
			if (i->screen->cinema == ck.cinema) {
				ck.screen_kdms.push_back (*i);
				list<ScreenKDM>::iterator j = i;
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

void
CinemaKDMs::write_zip_files (string filename_first_part, list<CinemaKDMs> cinema_kdms, boost::filesystem::path directory)
{
	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {
		boost::filesystem::path path = directory;
		path /= tidy_for_filename (i.cinema->name) + ".zip";
		i.make_zip_file (filename_first_part, path);
	}
}

/* XXX: should probably get from/to from the KDMs themselves */
void
CinemaKDMs::email (string filename_first_part, string cpl_name, list<CinemaKDMs> cinema_kdms, dcp::LocalTime from, dcp::LocalTime to)
{
	BOOST_FOREACH (CinemaKDMs const & i, cinema_kdms) {

		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path ();
		zip_file /= boost::filesystem::unique_path().string() + ".zip";
		i.make_zip_file (filename_first_part, zip_file);

		/* Send email */

		quickmail_initialize ();

		SafeStringStream start;
		start << from.date() << " " << from.time_of_day();
		SafeStringStream end;
		end << to.date() << " " << to.time_of_day();

		string subject = Config::instance()->kdm_subject();
		boost::algorithm::replace_all (subject, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (subject, "$START_TIME", start.str ());
		boost::algorithm::replace_all (subject, "$END_TIME", end.str ());
		boost::algorithm::replace_all (subject, "$CINEMA_NAME", i.cinema->name);
		quickmail mail = quickmail_create (Config::instance()->kdm_from().c_str(), subject.c_str ());

		quickmail_add_to (mail, i.cinema->email.c_str ());
		if (!Config::instance()->kdm_cc().empty ()) {
			quickmail_add_cc (mail, Config::instance()->kdm_cc().c_str ());
		}
		if (!Config::instance()->kdm_bcc().empty ()) {
			quickmail_add_bcc (mail, Config::instance()->kdm_bcc().c_str ());
		}

		string body = Config::instance()->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (body, "$START_TIME", start.str ());
		boost::algorithm::replace_all (body, "$END_TIME", end.str ());
		boost::algorithm::replace_all (body, "$CINEMA_NAME", i.cinema->name);

		SafeStringStream screens;
		BOOST_FOREACH (ScreenKDM const & j, i.screen_kdms) {
			screens << j.screen->name << ", ";
		}
		boost::algorithm::replace_all (body, "$SCREENS", screens.str().substr (0, screens.str().length() - 2));

		quickmail_set_body (mail, body.c_str());
		quickmail_add_attachment_file (mail, zip_file.string().c_str(), "application/zip");

		char const* error = quickmail_send (
			mail,
			Config::instance()->mail_server().c_str(),
			Config::instance()->mail_port(),
			Config::instance()->mail_user().c_str(),
			Config::instance()->mail_password().c_str()
			);

		if (error) {
			quickmail_destroy (mail);
			throw KDMError (
				String::compose (
					"Failed to send KDM email to %1 (%2)",
					Config::instance()->mail_server(),
					error
					)
				);
		}
		quickmail_destroy (mail);
	}
}
