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

/** @param cinema_kdms KDMs to email.
 *  @param container_name_format Format to ues for folders / ZIP files.
 *  @param filename_format Format to use for filenames.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 *  @param log Log to write to, or 0.
 */
SendKDMEmailJob::SendKDMEmailJob (
	list<CinemaKDMs> cinema_kdms,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	string cpl_name,
	shared_ptr<Log> log
	)
	: Job (shared_ptr<Film>())
	, _container_name_format (container_name_format)
	, _filename_format (filename_format)
	, _name_values (name_values)
	, _cpl_name (cpl_name)
	, _cinema_kdms (cinema_kdms)
	, _log (log)
{

}

string
SendKDMEmailJob::name () const
{
	dcp::NameFormat::Map::const_iterator i = _name_values.find ('f');
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
	CinemaKDMs::email (_cinema_kdms, _container_name_format, _filename_format, _name_values, _cpl_name, _log);
	set_progress (1);
	set_state (FINISHED_OK);
}
