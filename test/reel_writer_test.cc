/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

/** @file  test/reel_writer_test.cc
 *  @brief Test ReelWriter class.
 *  @ingroup selfcontained
 */

#include "lib/reel_writer.h"
#include "lib/film.h"
#include "lib/cross.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::string;
using boost::shared_ptr;
using boost::optional;

static bool equal (dcp::FrameInfo a, ReelWriter const & writer, shared_ptr<InfoFileHandle> file, Frame frame, Eyes eyes)
{
	dcp::FrameInfo b = writer.read_frame_info(file, frame, eyes);
	return a.offset == b.offset && a.size == b.size && a.hash == b.hash;
}

BOOST_AUTO_TEST_CASE (write_frame_info_test)
{
	shared_ptr<Film> film = new_test_film2 ("write_frame_info_test");
	dcpomatic::DCPTimePeriod const period (dcpomatic::DCPTime(0), dcpomatic::DCPTime(96000));
	ReelWriter writer (film, period, shared_ptr<Job>(), 0, 1, optional<string>());

	/* Write the first one */

	dcp::FrameInfo info1(0, 123, "12345678901234567890123456789012");
	writer.write_frame_info (0, EYES_LEFT, info1);
	shared_ptr<InfoFileHandle> file = film->info_file_handle(period, true);

	BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));

	/* Write some more */

	dcp::FrameInfo info2(596, 14921, "123acb789f1234ae782012n456339522");
	writer.write_frame_info (5, EYES_RIGHT, info2);

	BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
	BOOST_CHECK (equal(info2, writer, file, 5, EYES_RIGHT));

	dcp::FrameInfo info3(12494, 99157123, "xxxxyyyyabc12356ffsfdsf456339522");
	writer.write_frame_info (10, EYES_LEFT, info3);

	BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
	BOOST_CHECK (equal(info2, writer, file, 5, EYES_RIGHT));
	BOOST_CHECK (equal(info3, writer, file, 10, EYES_LEFT));

	/* Overwrite one */

	dcp::FrameInfo info4(55512494, 123599157123, "ABCDEFGyabc12356ffsfdsf4563395ZZ");
	writer.write_frame_info (5, EYES_RIGHT, info4);

	BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
	BOOST_CHECK (equal(info4, writer, file, 5, EYES_RIGHT));
	BOOST_CHECK (equal(info3, writer, file, 10, EYES_LEFT));
}
