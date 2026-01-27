/*
    Copyright (C) 2021-2022 Carl Hetherington <cth@carlh.net>

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
#include "dcp_transcode_job.h"
#include "film.h"
#include "job_manager.h"
#include "upload_job.h"
#include <dcp/cpl.h>
#include <dcp/search.h>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::vector;


DCPTranscodeJob::DCPTranscodeJob (shared_ptr<const Film> film, ChangedBehaviour changed)
	: TranscodeJob (film, changed)
{

}


void
DCPTranscodeJob::post_transcode ()
{
	if (Config::instance()->upload_after_make_dcp()) {
		JobManager::instance()->add(make_shared<UploadJob>(_film->dir(_film->dcp_name())));
	}

	/* The first directory is the project's DCP, so the first CPL will also be from the project
	 * (not from one of the DCPs imported into the project).
	 */
	vector<boost::filesystem::path> all_directories = { _film->dir(_film->dcp_name()) };

	vector<dcp::EncryptedKDM> all_kdms;
	for (auto content: _film->content()) {
		if (auto dcp_content = dynamic_pointer_cast<DCPContent>(content)) {
			auto directories = dcp_content->directories();
			std::copy (directories.begin(), directories.end(), std::back_inserter(all_directories));
			if (dcp_content->kdm()) {
				all_kdms.push_back (dcp_content->kdm().get());
			}
		}
	}

	auto cpls = dcp::find_and_resolve_cpls (all_directories, true);
	DCPOMATIC_ASSERT (!cpls.empty());
	auto cpl = cpls.front ();

	for (auto const& kdm: all_kdms) {
		cpl->add (decrypt_kdm_with_helpful_error(kdm));
	}

	write_dcp_digest_file (_film->file(cpl->annotation_text().get_value_or(cpl->id()) + ".dcpdig"), cpl, _film->key().hex());
}

