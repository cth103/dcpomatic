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

/** @file src/copy_from_dvd_job.cc
 *  @brief A job to copy a film from a DVD.
 */

#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "copy_from_dvd_job.h"
#include "film_state.h"
#include "dvd.h"
#include "cross.h"

using namespace std;
using namespace boost;

/** @param fs FilmState for the film to write DVD data into.
 *  @param l Log that we can write to.
 */
CopyFromDVDJob::CopyFromDVDJob (shared_ptr<const FilmState> fs, Log* l, shared_ptr<Job> req)
	: Job (fs, shared_ptr<Options> (), l, req)
{

}

string
CopyFromDVDJob::name () const
{
	return "Copy film from DVD";
}

void
CopyFromDVDJob::run ()
{
	/* Remove any old DVD rips */
	filesystem::remove_all (_fs->dir ("dvd"));

	string const dvd = find_dvd ();
	if (dvd.empty ()) {
		set_error ("could not find DVD");
		set_state (FINISHED_ERROR);
	}

	list<DVDTitle> const t = dvd_titles (dvd);
	if (t.empty ()) {
		set_error ("no titles found on DVD");
		set_state (FINISHED_ERROR);
	}

	int longest_title = 0;
	uint64_t longest_size = 0;
	for (list<DVDTitle>::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (longest_size < i->size) {
			longest_size = i->size;
			longest_title = i->number;
		}
	}

	stringstream c;
	c << "vobcopy -n " << longest_title << " -l -o \"" << _fs->dir ("dvd") << "\" 2>&1";

	FILE* f = popen (c.str().c_str(), "r");
	if (f == 0) {
		set_error ("could not run vobcopy command");
		set_state (FINISHED_ERROR);
		return;
	}

	while (!feof (f)) {
		char buf[256];
		if (fscanf (f, "%s", buf)) {
			string s (buf);
			if (!s.empty () && s[s.length() - 1] == '%') {
				set_progress (atof (s.substr(0, s.length() - 1).c_str()) / 100.0);
			}
		}
	}

	int const r = pclose (f);
	if (WEXITSTATUS (r) != 0) {
		set_error ("call to vobcopy failed");
		set_state (FINISHED_ERROR);
	} else {
		set_state (FINISHED_OK);
	}
}
