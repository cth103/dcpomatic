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

/** @file src/transcode_job.cc
 *  @brief A job which transcodes from one format to another.
 */

#include <iostream>
#include <iomanip>
#include "transcode_job.h"
#include "film.h"
#include "transcoder.h"
#include "log.h"
#include "safe_stringstream.h"

#include "i18n.h"

#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, Log::TYPE_GENERAL);
#define LOG_ERROR_NC(...)   _film->log()->log (__VA_ARGS__, Log::TYPE_ERROR);

using std::string;
using std::fixed;
using std::setprecision;
using std::cout;
using boost::shared_ptr;

/** @param s Film to use.
 */
TranscodeJob::TranscodeJob (shared_ptr<const Film> f)
	: Job (f)
{
	
}

string
TranscodeJob::name () const
{
	return String::compose (_("Transcode %1"), _film->name());
}

void
TranscodeJob::run ()
{
	try {

		LOG_GENERAL_NC (N_("Transcode job starting"));

		_transcoder.reset (new Transcoder (_film, shared_from_this ()));
		_transcoder->go ();
		set_progress (1);
		set_state (FINISHED_OK);

		LOG_GENERAL_NC (N_("Transcode job completed successfully"));
		_transcoder.reset ();

	} catch (...) {
		_transcoder.reset ();
		throw;
	}
}

string
TranscodeJob::status () const
{
	if (!_transcoder) {
		return Job::status ();
	}

	float const fps = _transcoder->current_encoding_rate ();
	if (fps == 0) {
		return Job::status ();
	}

	SafeStringStream s;

	s << Job::status ();

	if (!finished () && !_transcoder->finishing ()) {
		/// TRANSLATORS: fps here is an abbreviation for frames per second
		s << "; " << fixed << setprecision (1) << fps << " " << _("fps");
	}
	
	return s.str ();
}

/** @return Approximate remaining time in seconds */
int
TranscodeJob::remaining_time () const
{
	/* _transcoder might be destroyed by the job-runner thread */
	shared_ptr<Transcoder> t = _transcoder;
	
	if (!t) {
		return 0;
	}
	
	float fps = t->current_encoding_rate ();

	if (fps == 0) {
		return 0;
	}

	/* Compute approximate proposed length here, as it's only here that we need it */
	return (_film->length().frames (_film->video_frame_rate ()) - t->video_frames_out()) / fps;
}
