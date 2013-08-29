/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <vector>
#include <list>
#include <libdcp/dcp.h>
#include "lib/config.h"
#include "lib/util.h"
#include "lib/ui_signaller.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/cross.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dcpomatic_test
#include <boost/test/unit_test.hpp>

using std::string;
using std::vector;
using std::min;
using std::cout;
using std::cerr;
using std::list;
using boost::shared_ptr;

class TestUISignaller : public UISignaller
{
public:
	/* No wakes in tests: we call ui_idle ourselves */
	void wake_ui ()
	{

	}
};

struct TestConfig
{
	TestConfig()
	{
		dcpomatic_setup();

		Config::instance()->set_num_local_encoding_threads (1);
		Config::instance()->set_servers (vector<ServerDescription> ());
		Config::instance()->set_server_port (61920);
		Config::instance()->set_default_dci_metadata (DCIMetadata ());
		Config::instance()->set_default_container (static_cast<Ratio*> (0));
		Config::instance()->set_default_dcp_content_type (static_cast<DCPContentType*> (0));

		ui_signaller = new TestUISignaller ();
	}
};

BOOST_GLOBAL_FIXTURE (TestConfig);

boost::filesystem::path
test_film_dir (string name)
{
	boost::filesystem::path p;
	p /= "build";
	p /= "test";
	p /= name;
	return p;
}

shared_ptr<Film>
new_test_film (string name)
{
	boost::filesystem::path p = test_film_dir (name);
	if (boost::filesystem::exists (p)) {
		boost::filesystem::remove_all (p);
	}
	
	shared_ptr<Film> f = shared_ptr<Film> (new Film (p.string()));
	f->write_metadata ();
	return f;
}

static void
check_file (string ref, string check)
{
	uintmax_t N = boost::filesystem::file_size (ref);
	BOOST_CHECK_EQUAL (N, boost::filesystem::file_size(check));
	FILE* ref_file = fopen (ref.c_str(), "rb");
	BOOST_CHECK (ref_file);
	FILE* check_file = fopen (check.c_str(), "rb");
	BOOST_CHECK (check_file);
	
	int const buffer_size = 65536;
	uint8_t* ref_buffer = new uint8_t[buffer_size];
	uint8_t* check_buffer = new uint8_t[buffer_size];

	while (N) {
		uintmax_t this_time = min (uintmax_t (buffer_size), N);
		size_t r = fread (ref_buffer, 1, this_time, ref_file);
		BOOST_CHECK_EQUAL (r, this_time);
		r = fread (check_buffer, 1, this_time, check_file);
		BOOST_CHECK_EQUAL (r, this_time);

		BOOST_CHECK_EQUAL (memcmp (ref_buffer, check_buffer, this_time), 0);
		N -= this_time;
	}

	delete[] ref_buffer;
	delete[] check_buffer;

	fclose (ref_file);
	fclose (check_file);
}

static void
note (libdcp::NoteType, string n)
{
	cout << n << "\n";
}

void
check_dcp (string ref, string check)
{
	libdcp::DCP ref_dcp (ref);
	ref_dcp.read ();
	libdcp::DCP check_dcp (check);
	check_dcp.read ();

	libdcp::EqualityOptions options;
	options.max_mean_pixel_error = 5;
	options.max_std_dev_pixel_error = 5;
	options.max_audio_sample_error = 255;
	options.cpl_names_can_differ = true;
	options.mxf_names_can_differ = true;
	
	BOOST_CHECK (ref_dcp.equals (check_dcp, options, boost::bind (note, _1, _2)));
}

void
wait_for_jobs ()
{
	JobManager* jm = JobManager::instance ();
	while (jm->work_to_do ()) {
		ui_signaller->ui_idle ();
	}
	if (jm->errors ()) {
		for (list<shared_ptr<Job> >::iterator i = jm->_jobs.begin(); i != jm->_jobs.end(); ++i) {
			if ((*i)->finished_in_error ()) {
				cerr << (*i)->error_summary () << "\n"
				     << (*i)->error_details () << "\n";
			}
		}
	}
		
	BOOST_CHECK (!jm->errors());

	ui_signaller->ui_idle ();
}
