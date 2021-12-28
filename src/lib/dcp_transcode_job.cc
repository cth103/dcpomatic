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
#include "dcp_transcode_job.h"
#include "film.h"
#include "job_manager.h"
#include "upload_job.h"


using std::make_shared;
using std::shared_ptr;


DCPTranscodeJob::DCPTranscodeJob (shared_ptr<const Film> film, ChangedBehaviour changed)
	: TranscodeJob (film, changed)
{

}


void
DCPTranscodeJob::post_transcode ()
{
	if (Config::instance()->upload_after_make_dcp()) {
		JobManager::instance()->add(make_shared<UploadJob>(_film));
	}
}
