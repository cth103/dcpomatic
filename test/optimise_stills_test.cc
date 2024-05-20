/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/dcpomatic_log.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/ratio.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "lib/writer.h"
#include "test.h"
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::getline;
using std::ifstream;
using std::string;
using std::vector;
using boost::starts_with;
using boost::split;


static
void
check (string name, int check_full, int check_repeat)
{
	/* The encoder will have been destroyed so parse the logs */
	string log_file = "build/test/" + name + "/log";
	ifstream log (log_file.c_str());
	string line;
	int repeat = 0;
	int full = 0;
	while (getline (log, line)) {
		vector<string> bits;
		split (bits, line, boost::is_any_of (":"));
		if (bits.size() >= 4 && starts_with (bits[3], " Wrote")) {
			split (bits, bits[3], boost::is_any_of (" "));
			if (bits.size() >= 7) {
				full = atoi (bits[2].c_str());
				repeat = atoi (bits[6].c_str());
			}
		}

	}

	BOOST_CHECK_EQUAL (full, check_full);
	BOOST_CHECK_EQUAL (repeat, check_repeat);
}


/** Make a 2D DCP out of a 2D still and check that the J2K encoding is only done once for each frame */
BOOST_AUTO_TEST_CASE (optimise_stills_test1)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film2("optimise_stills_test1", { content });
	LogSwitcher ls (film->log());
	make_and_verify_dcp (film);

	check ("optimise_stills_test1", 1, 10 * 24 - 1);
}


/** Make a 3D DCP out of a 3D L/R still and check that the J2K encoding is only done once for L and R */
BOOST_AUTO_TEST_CASE (optimise_stills_test2)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film2("optimise_stills_test2", { content });
	LogSwitcher ls (film->log());
	content->video->set_frame_type (VideoFrameType::THREE_D_LEFT_RIGHT);
	film->set_three_d (true);
	make_and_verify_dcp (film);

	check ("optimise_stills_test2", 2, 10 * 48 - 2);
}
