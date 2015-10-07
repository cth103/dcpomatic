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

#include "send_kdm_email_job.h"
#include "compose.hpp"
#include "film.h"
#include "cinema_kdms.h"
#include <list>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

SendKDMEmailJob::SendKDMEmailJob (
	string film_name,
	string cpl_name,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to,
	list<CinemaKDMs> cinema_kdms
	)
	: Job (shared_ptr<Film>())
	, _film_name (film_name)
	, _cpl_name (cpl_name)
	, _from (from)
	, _to (to)
	, _cinema_kdms (cinema_kdms)
{

}

string
SendKDMEmailJob::name () const
{
	if (_film_name.empty ()) {
		return _("Email KDMs");
	}

	return String::compose (_("Email KDMs for %1"), _film_name);
}

string
SendKDMEmailJob::json_name () const
{
	return N_("send_kdm_email");
}

void
SendKDMEmailJob::run ()
{
	set_progress_unknown ();
	CinemaKDMs::email (_film_name, _cpl_name, _cinema_kdms,	_from, _to);
	set_progress (1);
	set_state (FINISHED_OK);
}
