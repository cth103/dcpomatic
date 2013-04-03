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
#include "format.h"
#include "transcoder.h"
#include "log.h"
#include "encoder.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using std::fixed;
using std::setprecision;
using boost::shared_ptr;

/** @param s Film to use.
 */
TranscodeJob::TranscodeJob (shared_ptr<Film> f)
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

		_film->log()->log (N_("Transcode job starting"));
		_film->log()->log (String::compose (N_("Audio delay is %1ms"), _film->audio_delay()));

		_transcoder.reset (new Transcoder (_film, shared_from_this ()));
		_transcoder->go ();
		set_progress (1);
		set_state (FINISHED_OK);

		_film->log()->log (N_("Transcode job completed successfully"));

	} catch (std::exception& e) {

		set_progress (1);
		set_state (FINISHED_ERROR);
		_film->log()->log (String::compose (N_("Transcode job failed (%1)"), e.what()));

		throw;
	}
}

string
TranscodeJob::status () const
{
	if (!_transcoder) {
		return _("0%");
	}

	float const fps = _transcoder->current_encoding_rate ();
	if (fps == 0) {
		return Job::status ();
	}

	stringstream s;

	s << Job::status ();

	if (!finished ()) {
		s << N_("; ") << fixed << setprecision (1) << fps << N_(" ") << _("frames per second");
	}
	
	return s.str ();
}

int
TranscodeJob::remaining_time () const
{
	if (!_transcoder) {
		return 0;
	}
	
	float fps = _transcoder->current_encoding_rate ();

	if (fps == 0) {
		return 0;
	}

	if (!_film->video_length()) {
		return 0;
	}

	/* Compute approximate proposed length here, as it's only here that we need it */
	int length = _film->video_length();
	FrameRateConversion const frc (_film->video_frame_rate(), _film->dcp_frame_rate());
	if (frc.skip) {
		length /= 2;
	}
	/* If we are repeating it shouldn't affect transcode time, so don't take it into account */

	int const left = length - _transcoder->video_frames_out();
	return left / fps;
}
