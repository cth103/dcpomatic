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

#include <list>
#include <boost/shared_ptr.hpp>
#include <quickmail.h>
#include <zip.h>
#include <libdcp/kdm.h>
#include "kdm.h"
#include "cinema.h"
#include "exceptions.h"
#include "util.h"
#include "film.h"
#include "config.h"

using std::list;
using std::string;
using std::stringstream;
using boost::shared_ptr;

struct ScreenKDM
{
	ScreenKDM (shared_ptr<Screen> s, libdcp::KDM k)
		: screen (s)
		, kdm (k)
	{}
	
	shared_ptr<Screen> screen;
	libdcp::KDM kdm;
};

static string
kdm_filename (shared_ptr<const Film> film, ScreenKDM kdm)
{
	return tidy_for_filename (film->name()) + "_" + tidy_for_filename (kdm.screen->cinema->name) + "_" + tidy_for_filename (kdm.screen->name) + ".kdm.xml";
}

struct CinemaKDMs
{
	shared_ptr<Cinema> cinema;
	list<ScreenKDM> screen_kdms;

	void make_zip_file (shared_ptr<const Film> film, boost::filesystem::path zip_file) const
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
		
		for (list<ScreenKDM>::const_iterator i = screen_kdms.begin(); i != screen_kdms.end(); ++i) {
			shared_ptr<string> kdm (new string (i->kdm.as_xml ()));
			kdm_strings.push_back (kdm);
			
			struct zip_source* source = zip_source_buffer (zip, kdm->c_str(), kdm->length(), 0);
			if (!source) {
				throw StringError ("could not create ZIP source");
			}
			
			if (zip_add (zip, kdm_filename (film, *i).c_str(), source) == -1) {
				throw StringError ("failed to add KDM to ZIP archive");
			}
		}
		
		if (zip_close (zip) == -1) {
			throw StringError ("failed to close ZIP archive");
		}
	}
};

/* Not complete but sufficient for our purposes (we're using
   ScreenKDM in a list where all the screens will be unique).
*/
bool
operator== (ScreenKDM const & a, ScreenKDM const & b)
{
	return a.screen == b.screen;
}

static list<ScreenKDM>
make_screen_kdms (
	shared_ptr<const Film> film,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to
	)
{
	list<libdcp::KDM> kdms = film->make_kdms (screens, dcp, from, to);
	   
	list<ScreenKDM> screen_kdms;
	
	list<shared_ptr<Screen> >::iterator i = screens.begin ();
	list<libdcp::KDM>::iterator j = kdms.begin ();
	while (i != screens.end() && j != kdms.end ()) {
		screen_kdms.push_back (ScreenKDM (*i, *j));
		++i;
		++j;
	}

	return screen_kdms;
}

static list<CinemaKDMs>
make_cinema_kdms (
	shared_ptr<const Film> film,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to
	)
{
	list<ScreenKDM> screen_kdms = make_screen_kdms (film, screens, dcp, from, to);
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

/** @param from KDM from time in local time.
 *  @param to KDM to time in local time.
 */
void
write_kdm_files (
	shared_ptr<const Film> film,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to,
	boost::filesystem::path directory
	)
{
	list<ScreenKDM> screen_kdms = make_screen_kdms (film, screens, dcp, from, to);

	/* Write KDMs to the specified directory */
	for (list<ScreenKDM>::iterator i = screen_kdms.begin(); i != screen_kdms.end(); ++i) {
		boost::filesystem::path out = directory;
		out /= kdm_filename (film, *i);
		i->kdm.as_xml (out);
	}
}

void
write_kdm_zip_files (
	shared_ptr<const Film> film,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to,
	boost::filesystem::path directory
	)
{
	list<CinemaKDMs> cinema_kdms = make_cinema_kdms (film, screens, dcp, from, to);

	for (list<CinemaKDMs>::const_iterator i = cinema_kdms.begin(); i != cinema_kdms.end(); ++i) {
		boost::filesystem::path path = directory;
		path /= tidy_for_filename (i->cinema->name) + ".zip";
		i->make_zip_file (film, path);
	}
}

void
email_kdms (
	shared_ptr<const Film> film,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to
	)
{
	list<CinemaKDMs> cinema_kdms = make_cinema_kdms (film, screens, dcp, from, to);

	for (list<CinemaKDMs>::const_iterator i = cinema_kdms.begin(); i != cinema_kdms.end(); ++i) {
		
		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path ();
		zip_file /= boost::filesystem::unique_path().string() + ".zip";
		i->make_zip_file (film, zip_file);
		
		/* Send email */
		
		quickmail_initialize ();
		quickmail mail = quickmail_create (Config::instance()->kdm_from().c_str(), "KDM delivery");
		quickmail_add_to (mail, i->cinema->email.c_str ());
		
		string body = Config::instance()->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", film->dcp_name ());
		stringstream start;
		start << from.date() << " " << from.time_of_day();
		boost::algorithm::replace_all (body, "$START_TIME", start.str ());
		stringstream end;
		end << to.date() << " " << to.time_of_day();
		boost::algorithm::replace_all (body, "$END_TIME", end.str ());

		quickmail_set_body (mail, body.c_str());
		quickmail_add_attachment_file (mail, zip_file.string().c_str(), "application/zip");

		int const port = Config::instance()->mail_user().empty() ? 25 : 587;

		char const* error = quickmail_send (
			mail,
			Config::instance()->mail_server().c_str(),
			port,
			Config::instance()->mail_user().c_str(),
			Config::instance()->mail_password().c_str()
			);
		
		if (error) {
			quickmail_destroy (mail);
			throw KDMError (String::compose ("Failed to send KDM email (%1)", error));
		}
		quickmail_destroy (mail);
	}
}
