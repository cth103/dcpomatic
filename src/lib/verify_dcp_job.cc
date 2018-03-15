/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "verify_dcp_job.h"
#include "content.h"

#include "i18n.h"

using std::string;
using std::vector;
using boost::shared_ptr;

VerifyDCPJob::VerifyDCPJob (vector<boost::filesystem::path> directories)
	: Job (shared_ptr<Film>())
	, _directories (directories)
{

}

string
VerifyDCPJob::name () const
{
	return _("Verify DCP");
}

string
VerifyDCPJob::json_name () const
{
	return N_("verify_dcp");
}

void
VerifyDCPJob::run ()
{
	_notes = dcp::verify (_directories);

	bool failed = false;
	BOOST_FOREACH (dcp::VerificationNote i, _notes) {
		if (i.type() == dcp::VerificationNote::ERROR) {
			failed = true;
		}
	}

	set_progress (1);
	set_state (failed ? FINISHED_ERROR : FINISHED_OK);
}
