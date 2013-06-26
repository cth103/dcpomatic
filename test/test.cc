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

#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time.hpp>
#include <libdcp/dcp.h>
#include "ratio.h"
#include "film.h"
#include "filter.h"
#include "job_manager.h"
#include "util.h"
#include "exceptions.h"
#include "image.h"
#include "log.h"
#include "dcp_video_frame.h"
#include "config.h"
#include "server.h"
#include "cross.h"
#include "job.h"
#include "subtitle.h"
#include "scaler.h"
#include "ffmpeg_decoder.h"
#include "sndfile_decoder.h"
#include "dcp_content_type.h"
#include "ui_signaller.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dcpomatic_test
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using std::stringstream;
using std::vector;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::thread;
using boost::dynamic_pointer_cast;

struct TestConfig
{
	TestConfig()
	{
		dcpomatic_setup();

		Config::instance()->set_num_local_encoding_threads (1);
		Config::instance()->set_servers (vector<ServerDescription*> ());
		Config::instance()->set_server_port (61920);
		Config::instance()->set_default_dci_metadata (DCIMetadata ());
		Config::instance()->set_default_container (0);
		Config::instance()->set_default_dcp_content_type (0);

		ui_signaller = new UISignaller ();
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

void
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

#include "ffmpeg_pts_offset.cc"
#include "ffmpeg_examiner_test.cc"
#include "black_fill_test.cc"
#include "scaling_test.cc"
#include "ratio_test.cc"
#include "pixel_formats_test.cc"
#include "make_black_test.cc"
#include "film_metadata_test.cc"
#include "stream_test.cc"
#include "util_test.cc"
#include "ffmpeg_dcp_test.cc"
#include "frame_rate_test.cc"
#include "job_test.cc"
#include "client_server_test.cc"
#include "image_test.cc"
