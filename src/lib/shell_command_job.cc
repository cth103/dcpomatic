/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/shell_command_job.cc
 *  @brief A job which calls a command via a shell.
 */

#include <stdio.h>
#include "shell_command_job.h"
#include "log.h"

using namespace std;
using namespace boost;

/** @param s Our FilmState.
 *  @param o Options.
 *  @param l Log.
 */
ShellCommandJob::ShellCommandJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Job (s, o, l)
{

}

/** Run a command via a shell.
 *  @param c Command to run.
 */
void
ShellCommandJob::command (string c)
{
	_log->log ("Command: " + c, Log::VERBOSE);
	
	int const r = system (c.c_str());
	if (r < 0) {
		set_state (FINISHED_ERROR);
		return;
	} else if (WEXITSTATUS (r) != 0) {
		set_error ("command failed");
		set_state (FINISHED_ERROR);
	} else {
		set_state (FINISHED_OK);
	}
}

string
ShellCommandJob::status () const
{
	if (!running () && !finished()) {
		return "Waiting";
	} else if (running ()) {
		return "";
	}

	return Job::status ();
}
