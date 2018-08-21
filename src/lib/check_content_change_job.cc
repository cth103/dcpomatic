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
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;

CheckContentChangeJob::CheckContentChangeJob (shared_ptr<const Film> film)
	: Job (film)
{

}

string
CheckContentChangeJob::name () const
{
	return _("Check content for changes");
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

	list<shared_ptr<Content> > changed;

	BOOST_FOREACH (shared_ptr<Content> i, _film->content()) {
		bool ic = false;
		for (size_t j = 0; j < i->number_of_paths(); ++j) {
			cout << boost::filesystem::last_write_time(i->path(j)) << " " << i->last_write_time(j) << "\n";
			if (boost::filesystem::last_write_time(i->path(j)) != i->last_write_time(j)) {
				cout << "last write differs.\n";
				ic = true;
				break;
			}
		}
		if (!ic && i->calculate_digest() != i->digest()) {
			cout << "digest differs.\n";
			ic = true;
		}
		if (ic) {
			cout << i->path(0) << " changed.\n";
			changed.push_back (i);
		}
	}

	BOOST_FOREACH (shared_ptr<Content> i, changed) {
		JobManager::instance()->add(shared_ptr<Job>(new ExamineContentJob(_film, i)));
	}

	set_progress (1);
	set_state (FINISHED_OK);
}
