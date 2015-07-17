/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/upload_job.cc
 *  @brief A job to copy DCPs to a server using libcurl.
 */

#include "compose.hpp"
#include "upload_job.h"
#include "config.h"
#include "log.h"
#include "film.h"
#include "scp_uploader.h"
#include "curl_uploader.h"
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, Log::TYPE_GENERAL);

using std::string;
using std::min;
using boost::shared_ptr;
using boost::scoped_ptr;

UploadJob::UploadJob (shared_ptr<const Film> film)
	: Job (film)
	, _status (_("Waiting"))
{

}

string
UploadJob::name () const
{
	return _("Copy DCP to TMS");
}

string
UploadJob::json_name () const
{
	return N_("upload");
}

void
UploadJob::run ()
{
	LOG_GENERAL_NC (N_("Upload job starting"));

	scoped_ptr<Uploader> uploader;
	switch (Config::instance()->tms_protocol ()) {
	case PROTOCOL_SCP:
		uploader.reset (new SCPUploader (bind (&UploadJob::set_status, this, _1), bind (&UploadJob::set_progress, this, _1, false)));
		break;
	case PROTOCOL_FTP:
		uploader.reset (new CurlUploader (bind (&UploadJob::set_status, this, _1), bind (&UploadJob::set_progress, this, _1, false)));
		break;
	}

	uploader->upload (_film->dir (_film->dcp_name ()));

	set_progress (1);
	set_status (N_(""));
	set_state (FINISHED_OK);
}

string
UploadJob::status () const
{
	boost::mutex::scoped_lock lm (_status_mutex);
	string s = Job::status ();
	if (!_status.empty () && !finished_in_error ()) {
		s += N_("; ") + _status;
	}
	return s;
}

void
UploadJob::set_status (string s)
{
	boost::mutex::scoped_lock lm (_status_mutex);
	_status = s;
}
