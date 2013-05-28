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
#include "format.h"
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
#include "container.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dcpomatic_test
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using std::stringstream;
using std::vector;
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

#include "container_test.cc"
#include "pixel_formats_test.cc"
#include "make_black_test.cc"
#include "film_metadata_test.cc"
#include "stream_test.cc"
#include "format_test.cc"
#include "util_test.cc"
#include "dcp_test.cc"
#include "frame_rate_test.cc"
#include "job_test.cc"
#include "client_server_test.cc"
#include "image_test.cc"
