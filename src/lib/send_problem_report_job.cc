/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
#include "cross.h"
#include "emailer.h"
#include "environment_info.h"
#include "film.h"
#include "film.h"
#include "log.h"
#include "send_problem_report_job.h"
#include "version.h"
#include <libxml++/libxml++.h>

#include "i18n.h"


using std::list;
using std::shared_ptr;
using std::string;


/** @param film Film thta the problem is with, or 0.
 *  @param from Email address to use for From:
 *  @param summary Summary of the problem.
 */
SendProblemReportJob::SendProblemReportJob (
	shared_ptr<const Film> film,
	string from,
	string summary
	)
	: Job (film)
	, _from (from)
	, _summary (summary)
{

}


SendProblemReportJob::~SendProblemReportJob ()
{
	stop_thread ();
}


string
SendProblemReportJob::name () const
{
	if (!_film) {
		return _("Email problem report");
	}

	return String::compose (_("Email problem report for %1"), _film->name());
}


string
SendProblemReportJob::json_name () const
{
	return N_("send_problem_report");
}


void
SendProblemReportJob::run ()
{
	sub (_("Sending email"));
	set_progress_unknown ();

	string body = _summary + "\n\n";

	body += "Version: " + string(dcpomatic_version) + " " + string(dcpomatic_git_commit) + "\n\n";

	for (auto i: environment_info ()) {
		body += i + "\n";
	}

	body += "\n";

	if (_film) {
		body += "log head and tail:\n";
		body += "---<8----\n";
		body += _film->log()->head_and_tail (4096);
		body += "---<8----\n\n";

		add_file (body, "ffprobe.log");

		body += "---<8----\n";
		body += _film->metadata()->write_to_string_formatted("UTF-8");
		body += "---<8----\n";
	}

	Emailer emailer (_from, {"carl@dcpomatic.com"}, "DCP-o-matic problem report", body);
	emailer.send ("main.carlh.net", 2525, EmailProtocol::STARTTLS);

	set_progress (1);
	set_state (FINISHED_OK);
}


void
SendProblemReportJob::add_file (string& body, boost::filesystem::path file) const
{
	body += file.string() + ":\n";
	body += "---<8----\n";
	try {
		body += dcp::file_to_string (_film->file(file));
	} catch (...) {
		body += "[could not be read]\n";
	}
	body += "---<8----\n\n";
}
