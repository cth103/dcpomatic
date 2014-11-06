/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "send_problem_report_job.h"
#include "compose.hpp"
#include "film.h"
#include "cross.h"
#include "film.h"
#include "log.h"
#include <quickmail.h>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

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
	return String::compose (_("Email problem report for %1"), _film->name());
}

void
SendProblemReportJob::run ()
{
	set_progress_unknown ();
	
	quickmail mail = quickmail_create (_from.c_str(), "DCP-o-matic problem report");
	
	quickmail_add_to (mail, "carl@dcpomatic.com");
	
	string body = _summary;
	
	body += "log head and tail:\n";
	body += "---<8----\n";
	body += _film->log()->head_and_tail ();
	body += "---<8----\n\n";
	
	FILE* ffprobe = fopen_boost (_film->file ("ffprobe.log"), "r");
	if (ffprobe) {
		body += "ffprobe.log:\n";
		body += "---<8----\n";
		uintmax_t const size = boost::filesystem::file_size (_film->file ("ffprobe.log"));
		char* buffer = new char[size + 1];
		int const N = fread (buffer, size, 1, ffprobe);
		buffer[N] = '\0';
		body += buffer;
		delete[] buffer;
		body += "---<8----\n\n";
		fclose (ffprobe);
	}
	
	quickmail_set_body (mail, body.c_str());
	
	char const* error = quickmail_send (mail, "main.carlh.net", 2525, 0, 0);
	
	if (error) {
		set_state (FINISHED_ERROR);
		set_error (error, "");
	} else {
		set_state (FINISHED_OK);
	}
	
	quickmail_destroy (mail);

	set_progress (1);
}
