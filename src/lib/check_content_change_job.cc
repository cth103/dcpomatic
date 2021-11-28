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

#include "check_content_change_job.h"
#include "job_manager.h"
#include "examine_content_job.h"
#include "content.h"
#include "film.h"
#include <iostream>

#include "i18n.h"

using std::string;
using std::list;
using std::cout;
using std::shared_ptr;

CheckContentChangeJob::CheckContentChangeJob (shared_ptr<const Film> film)
	: Job (film)
{

}

CheckContentChangeJob::~CheckContentChangeJob ()
{
	stop_thread ();
}

string
CheckContentChangeJob::name () const
{
	return _("Checking content for changes");
}

string
CheckContentChangeJob::json_name () const
{
	return N_("check_content_change");
}

void
CheckContentChangeJob::run ()
{
	set_progress_unknown ();

	auto content = _film->content();
	std::vector<shared_ptr<Content>> changed;
	std::copy_if (content.begin(), content.end(), std::back_inserter(changed), [](shared_ptr<Content> c) { return c->changed(); });

	if (!changed.empty()) {
		for (auto i: changed) {
			JobManager::instance()->add(make_shared<ExamineContentJob>(_film, i));
		}
		set_message (_("Some files have been changed since they were added to the project.\n\nThese files will now be re-examined, so you may need to check their settings."));
	}

	set_progress (1);
	set_state (FINISHED_OK);
}
