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
#include "dcp_digest_file.h"
#include "film.h"
#include "job_manager.h"
#include "upload_job.h"
#include <dcp/cpl.h>
#include <dcp/search.h>


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

	dcp::DCP dcp(_film->dir(_film->dcp_name()));
	dcp.read();

	for (auto cpl: dcp.cpls()) {
		write_dcp_digest_file (_film->file(cpl->annotation_text().get_value_or(cpl->id()) + ".dcpdig"), cpl, _film->key().hex());
	}
}

