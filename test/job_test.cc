/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/job_test.cc
 *  @brief Basic tests of Job and JobManager.
 */

#include <boost/test/unit_test.hpp>
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/cross.h"

using std::string;
using boost::shared_ptr;

class TestJob : public Job
{
public:
	TestJob (shared_ptr<Film> f)
		: Job (f)
	{

	}

	void set_finished_ok () {
		set_state (FINISHED_OK);
	}

	void set_finished_error () {
		set_state (FINISHED_ERROR);
	}

	void run ()
	{
		while (true) {
			if (finished ()) {
				return;
			}
		}
	}

	string name () const {
		return "";
	}

	string json_name () const {
		return "";
	}
};

BOOST_AUTO_TEST_CASE (job_manager_test)
{
	shared_ptr<Film> f;

	/* Single job */
	shared_ptr<TestJob> a (new TestJob (f));

	JobManager::instance()->add (a);
	dcpomatic_sleep (1);
	BOOST_CHECK_EQUAL (a->running (), true);
	a->set_finished_ok ();
	dcpomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->finished_ok(), true);
}
