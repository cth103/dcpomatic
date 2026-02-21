/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include "check_content_job.h"
#include "content.h"
#include "dcp_content.h"
#include "examine_content_job.h"
#include "film.h"
#include "ffmpeg_content.h"
#include "job_manager.h"
#include "string_text_file_content.h"
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;


CheckContentJob::CheckContentJob(shared_ptr<const Film> film)
	: Job(film)
{

}

CheckContentJob::~CheckContentJob()
{
	stop_thread();
}

string
CheckContentJob::name() const
{
	return _("Checking content");
}

string
CheckContentJob::json_name() const
{
	return N_("check_content");
}

void
CheckContentJob::run()
{
	set_progress_unknown();

	auto content = _film->content();

	if (_film->last_written_by_earlier_than(2, 17, 17)) {
		for (auto c: content) {
			if (auto stf = dynamic_pointer_cast<StringTextFileContent>(c)) {
				stf->check_font_ids();
			} else if (auto dcp = dynamic_pointer_cast<DCPContent>(c)) {
				dcp->check_font_ids();
			}
		}
	}

	std::vector<shared_ptr<Content>> changed;
	std::copy_if(content.begin(), content.end(), std::back_inserter(changed), [](shared_ptr<Content> c) { return c->changed(); });
	if (!changed.empty()) {
		JobManager::instance()->add(make_shared<ExamineContentJob>(_film, changed, false));
		set_message(_("Some files have been changed since they were added to the project.\n\nThese files will now be re-examined, so you may need to check their settings."));
	}

	if (!_film->last_written_by_git() && _film->last_written_by_earlier_than(2, 18, 30)) {
		std::vector<shared_ptr<Content>> needs_upgrade;
		std::copy_if(content.begin(), content.end(), std::back_inserter(needs_upgrade), [](shared_ptr<Content> c) { return static_cast<bool>(dynamic_pointer_cast<FFmpegContent>(c)); });
		if (!needs_upgrade.empty()) {
			JobManager::instance()->add(make_shared<ExamineContentJob>(_film, needs_upgrade, false));
			set_message(_("Some files must be re-examined due to a bug fix in DCP-o-matic.  You may need to check their settings."));
		}
	}

	set_progress(1);
	set_state(FINISHED_OK);
}
