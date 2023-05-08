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


#include "compose.hpp"
#include "film.h"
#include "kdm_with_metadata.h"
#include "send_kdm_email_job.h"
#include <list>

#include "i18n.h"


using std::list;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


SendKDMEmailJob::SendKDMEmailJob (
	list<KDMWithMetadataPtr> kdms,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	string cpl_name,
	vector<string> extra_addresses
	)
	: Job (shared_ptr<Film>())
	, _container_name_format (container_name_format)
	, _filename_format (filename_format)
	, _cpl_name (cpl_name)
	, _extra_addresses (extra_addresses)
{
	for (auto i: kdms) {
		list<KDMWithMetadataPtr> s;
		s.push_back (i);
		_kdms.push_back (s);
	}
}


/** @param kdms KDMs to email.
 *  @param container_name_format Format to use for folders / ZIP files.
 *  @param filename_format Format to use for filenames.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 */
SendKDMEmailJob::SendKDMEmailJob (
	list<list<KDMWithMetadataPtr> > kdms,
	dcp::NameFormat container_name_format,
	dcp::NameFormat filename_format,
	string cpl_name,
	vector<string> extra_addresses
	)
	: Job (shared_ptr<Film>())
	, _container_name_format (container_name_format)
	, _filename_format (filename_format)
	, _cpl_name (cpl_name)
	, _kdms (kdms)
	, _extra_addresses (extra_addresses)
{

}


SendKDMEmailJob::~SendKDMEmailJob ()
{
	stop_thread ();
}


string
SendKDMEmailJob::name () const
{
	auto f = _kdms.front().front()->get('f');
	if (!f || f->empty()) {
		return _("Email KDMs");
	}

	return String::compose (_("Email KDMs for %1"), *f);
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
	send_emails (_kdms, _container_name_format, _filename_format, _cpl_name, _extra_addresses);
	set_progress (1);
	set_state (FINISHED_OK);
}
