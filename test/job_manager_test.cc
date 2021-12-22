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


/** @file  test/job_manager_test.cc
 *  @brief Test JobManager.
 *  @ingroup selfcontained
 */


#include "lib/cross.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


class TestJob : public Job
{
public:
	explicit TestJob (shared_ptr<Film> film)
		: Job (film)
	{

	}

	~TestJob ()
	{
		stop_thread ();
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


BOOST_AUTO_TEST_CASE (job_manager_test1)
{
	shared_ptr<Film> film;

	/* Single job */
	auto a = make_shared<TestJob>(film);

	JobManager::instance()->add (a);
	dcpomatic_sleep_seconds (1);
	BOOST_CHECK (a->running());
	a->set_finished_ok ();
	dcpomatic_sleep_seconds (2);
	BOOST_CHECK (a->finished_ok());
}


BOOST_AUTO_TEST_CASE (job_manager_test2)
{
	shared_ptr<Film> film;

	vector<shared_ptr<TestJob>> jobs;
	for (int i = 0; i < 16; ++i) {
		auto job = make_shared<TestJob>(film);
		jobs.push_back (job);
		JobManager::instance()->add (job);
	}

	dcpomatic_sleep_seconds (1);
	BOOST_CHECK (jobs[0]->running());
	jobs[0]->set_finished_ok();

	dcpomatic_sleep_seconds (1);
	BOOST_CHECK (!jobs[0]->running());
	BOOST_CHECK (jobs[1]->running());

	/* Push our jobs[5] to the top of the list */
	for (int i = 0; i < 5; ++i) {
		JobManager::instance()->increase_priority(jobs[5]);
	}

	dcpomatic_sleep_seconds (1);
	for (int i = 0; i < 16; ++i) {
		BOOST_CHECK (i == 5 ? jobs[i]->running() : !jobs[i]->running());
	}

	/* Set any jobs that are started to be finished, until they're all finished */
	while (true) {
		if (std::find_if(jobs.begin(), jobs.end(), [](shared_ptr<Job> job) { return !job->finished_ok(); }) == jobs.end()) {
			break;
		}

		for (auto job: jobs) {
			if (job->running()) {
				job->set_finished_ok();
			}
		}
	}

	BOOST_REQUIRE (!wait_for_jobs());
}

