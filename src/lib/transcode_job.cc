/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "analytics.h"
#include "compose.hpp"
#include "content.h"
#include "config.h"
#include "dcp_encoder.h"
#include "dcpomatic_log.h"
#include "encoder.h"
#include "examine_content_job.h"
#include "film.h"
#include "job_manager.h"
#include "log.h"
#include "transcode_job.h"
#include "upload_job.h"
#include <iomanip>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::fixed;
using std::make_shared;
using std::setprecision;
using std::shared_ptr;
using std::string;
using boost::optional;
using std::dynamic_pointer_cast;


/** @param film Film to use */
TranscodeJob::TranscodeJob (shared_ptr<const Film> film, ChangedBehaviour changed)
	: Job (film)
	, _changed (changed)
{

}


TranscodeJob::~TranscodeJob ()
{
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
		auto content = _film->content();
		std::vector<shared_ptr<Content>> changed;
		std::copy_if (content.begin(), content.end(), std::back_inserter(changed), [](shared_ptr<Content> c) { return c->changed(); });

		if (!changed.empty()) {
			switch (_changed) {
			case ChangedBehaviour::EXAMINE_THEN_STOP:
				for (auto i: changed) {
					JobManager::instance()->add(make_shared<ExamineContentJob>(_film, i));
				}
				set_progress (1);
				set_message (_("Some files have been changed since they were added to the project.\n\nThese files will now be re-examined, so you may need to check their settings before trying again."));
				set_error (_("Files have changed since they were added to the project."), _("Check their new settings, then try again."));
				set_state (FINISHED_ERROR);
				return;
			case ChangedBehaviour::STOP:
				set_progress (1);
				set_error (_("Files have changed since they were added to the project."), _("Open the project in DCP-o-matic, check the settings, then save it before trying again."));
				set_state (FINISHED_ERROR);
				return;
			default:
				LOG_GENERAL_NC (_("Some files have been changed since they were added to the project."));
				break;
			}
		}

		LOG_GENERAL_NC (N_("Transcode job starting"));

		DCPOMATIC_ASSERT (_encoder);
		_encoder->go ();

		LOG_GENERAL(N_("Transcode job completed successfully: %1 fps"), dcp::locale_convert<string>(frames_per_second(), 2, true));

		if (dynamic_pointer_cast<DCPEncoder>(_encoder)) {
			try {
				Analytics::instance()->successful_dcp_encode();
			} catch (FileError& e) {
				LOG_WARNING (N_("Failed to write analytics (%1)"), e.what());
			}
		}

		post_transcode ();

		_encoder.reset ();

		set_progress (1);
		set_state (FINISHED_OK);

	} catch (...) {
		_encoder.reset ();
		throw;
	}
}

void TranscodeJob::pause() {
	_encoder->pause();
}

void TranscodeJob::resume() {
	_encoder->resume();
	Job::resume();
}

string
TranscodeJob::status () const
{
	if (!_encoder) {
		return Job::status ();
	}

	if (finished() || _encoder->finishing()) {
		return Job::status();
	}

	auto status = String::compose(_("%1; %2/%3 frames"), Job::status(), _encoder->frames_done(), _film->length().frames_round(_film->video_frame_rate()));
	if (auto const fps = _encoder->current_rate()) {
		/// TRANSLATORS: fps here is an abbreviation for frames per second
		status += String::compose(_("; %1 fps"), dcp::locale_convert<string>(*fps, 1, true));
	}

	return status;
}


/** @return Approximate remaining time in seconds */
int
TranscodeJob::remaining_time () const
{
	/* _encoder might be destroyed by the job-runner thread */
	auto e = _encoder;

	if (!e || e->finishing()) {
		/* We aren't doing any actual encoding so just use the job's guess */
		return Job::remaining_time ();
	}

	/* We're encoding so guess based on the current encoding rate */

	auto fps = e->current_rate ();

	if (!fps) {
		return 0;
	}

	/* Compute approximate proposed length here, as it's only here that we need it */
	return (_film->length().frames_round(_film->video_frame_rate()) - e->frames_done()) / *fps;
}


float
TranscodeJob::frames_per_second() const
{
	if (_finish_time != _start_time) {
		return _encoder->frames_done() / (_finish_time - _start_time);
	} else {
		return 0;
	}
}

