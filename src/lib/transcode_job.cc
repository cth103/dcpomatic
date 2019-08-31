/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

/** @file src/transcode_job.cc
 *  @brief A job which transcodes from one format to another.
 */

#include "transcode_job.h"
#include "dcp_encoder.h"
#include "upload_job.h"
#include "job_manager.h"
#include "film.h"
#include "encoder.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "compose.hpp"
#include "analytics.h"
#include <iostream>
#include <iomanip>

#include "i18n.h"

using std::string;
using std::fixed;
using std::setprecision;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

/** @param film Film to use */
TranscodeJob::TranscodeJob (shared_ptr<const Film> film)
	: Job (film)
{

}

TranscodeJob::~TranscodeJob ()
{
	/* We have to stop the job thread here as we're about to start tearing down
	   the Encoder, which is bad news if the job thread is still feeding it data.
	*/
	stop_thread ();
}

string
TranscodeJob::name () const
{
	return String::compose (_("Transcoding %1"), _film->name());
}

string
TranscodeJob::json_name () const
{
	return N_("transcode");
}

void
TranscodeJob::set_encoder (shared_ptr<Encoder> e)
{
	_encoder = e;
}

void
TranscodeJob::run ()
{
	try {
		struct timeval start;
		gettimeofday (&start, 0);
		LOG_GENERAL_NC (N_("Transcode job starting"));

		DCPOMATIC_ASSERT (_encoder);
		_encoder->go ();
		set_progress (1);
		set_state (FINISHED_OK);

		struct timeval finish;
		gettimeofday (&finish, 0);

		float fps = 0;
		if (finish.tv_sec != start.tv_sec) {
			fps = _encoder->frames_done() / (finish.tv_sec - start.tv_sec);
		}

		LOG_GENERAL (N_("Transcode job completed successfully: %1 fps"), fps);

		if (dynamic_pointer_cast<DCPEncoder>(_encoder)) {
			Analytics::instance()->successful_dcp_encode();
		}

		/* XXX: this shouldn't be here */
		if (_film->upload_after_make_dcp() && dynamic_pointer_cast<DCPEncoder>(_encoder)) {
			shared_ptr<Job> job (new UploadJob (_film));
			JobManager::instance()->add (job);
		}

		_encoder.reset ();

	} catch (...) {
		_encoder.reset ();
		throw;
	}
}

string
TranscodeJob::status () const
{
	if (!_encoder) {
		return Job::status ();
	}


	char buffer[256];
	if (finished() || _encoder->finishing()) {
		strncpy (buffer, Job::status().c_str(), 255);
		buffer[255] = '\0';
	} else {
		snprintf (
			buffer, sizeof(buffer), "%s; %" PRId64 "/%" PRId64 " frames",
			Job::status().c_str(),
			_encoder->frames_done(),
			_film->length().frames_round (_film->video_frame_rate ())
			);

		optional<float> const fps = _encoder->current_rate ();
		if (fps) {
			char fps_buffer[64];
			/// TRANSLATORS: fps here is an abbreviation for frames per second
			snprintf (fps_buffer, sizeof(fps_buffer), _("; %.1f fps"), *fps);
			strncat (buffer, fps_buffer, strlen(buffer) - 1);
		}
	}

	return buffer;
}

/** @return Approximate remaining time in seconds */
int
TranscodeJob::remaining_time () const
{
	/* _encoder might be destroyed by the job-runner thread */
	shared_ptr<Encoder> e = _encoder;

	if (!e || e->finishing()) {
		/* We aren't doing any actual encoding so just use the job's guess */
		return Job::remaining_time ();
	}

	/* We're encoding so guess based on the current encoding rate */

	optional<float> fps = e->current_rate ();

	if (!fps) {
		return 0;
	}

	/* Compute approximate proposed length here, as it's only here that we need it */
	return (_film->length().frames_round(_film->video_frame_rate()) - e->frames_done()) / *fps;
}
