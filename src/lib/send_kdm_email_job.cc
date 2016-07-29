/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "send_kdm_email_job.h"
#include "compose.hpp"
#include "film.h"
#include "cinema_kdms.h"
#include <list>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

/** @param log Log to write to, or 0 */
SendKDMEmailJob::SendKDMEmailJob (
	list<CinemaKDMs> cinema_kdms,
	KDMNameFormat name_format,
	dcp::NameFormat::Map name_values,
	string cpl_name,
	shared_ptr<Log> log
	)
	: Job (shared_ptr<Film>())
	, _name_format (name_format)
	, _name_values (name_values)
	, _cpl_name (cpl_name)
	, _cinema_kdms (cinema_kdms)
	, _log (log)
{

}

string
SendKDMEmailJob::name () const
{
	dcp::NameFormat::Map::const_iterator i = _name_values.find ("film_name");
	if (i == _name_values.end() || i->second.empty ()) {
		return _("Email KDMs");
	}

	return String::compose (_("Email KDMs for %1"), i->second);
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
	CinemaKDMs::email (_cinema_kdms, _name_format, _name_values, _cpl_name, _log);
	set_progress (1);
	set_state (FINISHED_OK);
}
