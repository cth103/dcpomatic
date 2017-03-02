/*
    Copyright (C) 2014-2017 Carl Hetherington <cth@carlh.net>

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

#include "send_problem_report_job.h"
#include "compose.hpp"
#include "film.h"
#include "cross.h"
#include "film.h"
#include "log.h"
#include "version.h"
#include "emailer.h"
#include "environment_info.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

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

	body += "Version: " + string (dcpomatic_version) + " " + string (dcpomatic_git_commit) + "\n\n";

	BOOST_FOREACH (string i, environment_info ()) {
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

	list<string> to;
	to.push_back ("carl@dcpomatic.com");

	Emailer emailer (_from, to, "DCP-o-matic problem report", body);
	emailer.send ("main.carlh.net", 2525);

	set_progress (1);
	set_state (FINISHED_OK);
}

void
SendProblemReportJob::add_file (string& body, boost::filesystem::path file) const
{
	FILE* f = fopen_boost (_film->file (file), "r");
	if (!f) {
		return;
	}

	body += file.string() + ":\n";
	body += "---<8----\n";
	uintmax_t const size = boost::filesystem::file_size (_film->file (file));
	char* buffer = new char[size + 1];
	int const N = fread (buffer, 1, size, f);
	buffer[N] = '\0';
	body += buffer;
	delete[] buffer;
	body += "---<8----\n\n";
	fclose (f);
}
