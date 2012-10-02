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

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include "check_hashes_job.h"
#include "film_state.h"
#include "options.h"
#include "log.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "transcode_job.h"

using namespace std;
using namespace boost;

CheckHashesJob::CheckHashesJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Job (s, o, l)
	, _bad (0)
{

}

string
CheckHashesJob::name () const
{
	return String::compose ("Check hashes of %1", _fs->name);
}

void
CheckHashesJob::run ()
{
	_bad = 0;
	
	for (int i = 0; i < _fs->length; ++i) {
		string const j2k_file = _opt->frame_out_path (i, false);
		string const hash_file = j2k_file + ".md5";

		ifstream ref (hash_file.c_str ());
		string hash;
		ref >> hash;

		if (hash != md5_digest (j2k_file)) {
			_log->log ("Frame " + lexical_cast<string> (i) + " has wrong hash; deleting.");
			filesystem::remove (j2k_file);
			filesystem::remove (hash_file);
			++_bad;
		}

		set_progress (float (i) / _fs->length);
	}

	if (_bad) {
		shared_ptr<Job> tc;

		if (_fs->dcp_ab) {
			tc.reset (new ABTranscodeJob (_fs, _opt, _log));
		} else {
			tc.reset (new TranscodeJob (_fs, _opt, _log));
		}
		
		JobManager::instance()->add_after (shared_from_this(), tc);
		JobManager::instance()->add_after (tc, shared_ptr<Job> (new CheckHashesJob (_fs, _opt, _log)));
	}
		
	set_progress (1);
	set_state (FINISHED_OK);
}

string
CheckHashesJob::status () const
{
	stringstream s;
	s << Job::status ();
	if (overall_progress() > 0) {
		if (_bad == 0) {
			s << "; no bad frames found";
		} else if (_bad == 1) {
			s << "; 1 bad frame found";
		} else {
			s << "; " << _bad << " bad frames found";
		}
	}
	return s.str ();
}
