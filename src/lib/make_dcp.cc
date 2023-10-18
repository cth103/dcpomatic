/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "dcp_content.h"
#include "dcp_encoder.h"
#include "dcp_transcode_job.h"
#include "dcpomatic_log.h"
#include "environment_info.h"
#include "film.h"
#include "job_manager.h"
#include "make_dcp.h"
#include <stdexcept>

#include "i18n.h"


using std::dynamic_pointer_cast;
using std::make_shared;
using std::runtime_error;
using std::shared_ptr;
using std::string;


/** Add suitable Job to the JobManager to create a DCP for a Film */
shared_ptr<TranscodeJob>
make_dcp (shared_ptr<Film> film, TranscodeJob::ChangedBehaviour behaviour)
{
	if (film->dcp_name().find("/") != string::npos) {
		throw BadSettingError (_("name"), _("Cannot contain slashes"));
	}

	if (film->container() == nullptr) {
		throw MissingSettingError (_("container"));
	}

	if (film->content().empty()) {
		throw runtime_error (_("You must add some content to the DCP before creating it"));
	}

	if (film->length() == dcpomatic::DCPTime()) {
		throw runtime_error (_("The DCP is empty, perhaps because all the content has zero length."));
	}

	if (film->dcp_content_type() == nullptr) {
		throw MissingSettingError (_("content type"));
	}

	if (film->name().empty()) {
		film->set_name ("DCP");
	}

	for (auto i: film->content()) {
		if (!i->paths_valid()) {
			throw runtime_error (_("Some of your content is missing"));
		}
		auto dcp = dynamic_pointer_cast<const DCPContent>(i);
		if (dcp && dcp->needs_kdm()) {
			throw runtime_error (_("Some of your content needs a KDM"));
		}
		if (dcp && dcp->needs_assets()) {
			throw runtime_error (_("Some of your content needs an OV"));
		}
	}

	film->set_isdcf_date_today ();

	for (auto info: environment_info()) {
		LOG_GENERAL_NC (info);
	}

	for (auto content: film->content()) {
		LOG_GENERAL ("Content: %1", content->technical_summary());
	}
	LOG_GENERAL ("DCP video rate %1 fps", film->video_frame_rate());
	LOG_GENERAL ("J2K bandwidth %1", film->j2k_bandwidth());

	auto tj = make_shared<DCPTranscodeJob>(film, behaviour);
	tj->set_encoder (make_shared<DCPEncoder>(film, tj));
	JobManager::instance()->add (tj);

	return tj;
}

