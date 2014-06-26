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
#include "kdm.h"

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

SendKDMEmailJob::SendKDMEmailJob (
	shared_ptr<const Film> f,
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	boost::posix_time::ptime from,
	boost::posix_time::ptime to,
	libdcp::KDM::Formulation formulation
	)
	: Job (f)
	, _screens (screens)
	, _dcp (dcp)
	, _from (from)
	, _to (to)
	, _formulation (formulation)
{

}

string
SendKDMEmailJob::name () const
{
	return String::compose (_("Email KDMs for %1"), _film->name());
}

string
SendKDMEmailJob::json_name () const
{
	return N_("send_kdm_email");
}

void
SendKDMEmailJob::run ()
{
	try {
		
		set_progress_unknown ();
		email_kdms (_film, _screens, _dcp, _from, _to, _formulation);
		set_progress (1);
		set_state (FINISHED_OK);
		
	} catch (std::exception& e) {

		set_progress (1);
		set_state (FINISHED_ERROR);
		throw;
	}
}
