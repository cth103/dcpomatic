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
#include "options.h"
#include "log.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "transcode_job.h"
#include "film.h"
#include "exceptions.h"

using std::string;
using std::stringstream;
using std::ifstream;
using boost::shared_ptr;

CheckHashesJob::CheckHashesJob (shared_ptr<Film> f, shared_ptr<const DecodeOptions> od, shared_ptr<const EncodeOptions> oe, shared_ptr<Job> req)
	: Job (f, req)
	, _decode_opt (od)
	, _encode_opt (oe)
	, _bad (0)
{

}

string
CheckHashesJob::name () const
{
	return String::compose ("Check hashes of %1", _film->name());
}

void
CheckHashesJob::run ()
{
	_bad = 0;

	if (!_film->dcp_length()) {
		throw EncodeError ("cannot check hashes of a DCP with unknown length");
	}
	
	SourceFrame const N = _film->dcp_trim_start() + _film->dcp_length().get();
	DCPFrameRate const dfr = dcp_frame_rate (_film->frames_per_second ());
	
	for (SourceFrame i = _film->dcp_trim_start(); i < N; i += dfr.skip) {
		string const j2k_file = _encode_opt->frame_out_path (i, false);
		string const hash_file = _encode_opt->hash_out_path (i, false);

		if (!boost::filesystem::exists (j2k_file)) {
			_film->log()->log (String::compose ("Frame %1 has a missing J2K file.", i));
			boost::filesystem::remove (hash_file);
			++_bad;
		} else if (!boost::filesystem::exists (hash_file)) {
			_film->log()->log (String::compose ("Frame %1 has a missing hash file.", i));
			boost::filesystem::remove (j2k_file);
			++_bad;
		} else {
			ifstream ref (hash_file.c_str ());
			string hash;
			ref >> hash;
			if (hash != md5_digest (j2k_file)) {
				_film->log()->log (String::compose ("Frame %1 has wrong hash; deleting.", i));
				boost::filesystem::remove (j2k_file);
				boost::filesystem::remove (hash_file);
				++_bad;
			}
		}

		set_progress (float (i) / N);
	}

	if (_bad) {
		shared_ptr<Job> tc;

		if (_film->dcp_ab()) {
			tc.reset (new ABTranscodeJob (_film, _decode_opt, _encode_opt, shared_from_this()));
		} else {
			tc.reset (new TranscodeJob (_film, _decode_opt, _encode_opt, shared_from_this()));
		}
		
		JobManager::instance()->add_after (shared_from_this(), tc);
		JobManager::instance()->add_after (tc, shared_ptr<Job> (new CheckHashesJob (_film, _decode_opt, _encode_opt, tc)));
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
