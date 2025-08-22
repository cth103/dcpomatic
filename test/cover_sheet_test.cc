/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/config.h"
#include "lib/cover_sheet.h"
#include "lib/dcpomatic_time.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(cover_sheet_test)
{
	ConfigRestorer cr;

	auto film = new_test_film("cover_sheet_test");
	film->set_video_frame_rate(24);

	film->set_marker(dcp::Marker::FFOC, dcpomatic::DCPTime::from_frames(24 *    6 +  9, 24));
	film->set_marker(dcp::Marker::LFOC, dcpomatic::DCPTime::from_frames(24 *   42 + 15, 24));
	film->set_marker(dcp::Marker::FFTC, dcpomatic::DCPTime::from_frames(24 *   95 +  4, 24));
	film->set_marker(dcp::Marker::LFTC, dcpomatic::DCPTime::from_frames(24 *  106 +  1, 24));
	film->set_marker(dcp::Marker::FFOI, dcpomatic::DCPTime::from_frames(24 *  112 +  0, 24));
	film->set_marker(dcp::Marker::LFOI, dcpomatic::DCPTime::from_frames(24 *  142 +  6, 24));
	film->set_marker(dcp::Marker::FFEC, dcpomatic::DCPTime::from_frames(24 *  216 + 23, 24));
	film->set_marker(dcp::Marker::LFEC, dcpomatic::DCPTime::from_frames(24 *  242 + 21, 24));
	film->set_marker(dcp::Marker::FFMC, dcpomatic::DCPTime::from_frames(24 *  250 + 23, 24));
	film->set_marker(dcp::Marker::LFMC, dcpomatic::DCPTime::from_frames(24 *  251 + 21, 24));

	Config::instance()->set_cover_sheet(
		"First frame of content: $FFOC\n"
		"Last frame of content: $LFOC\n"
		"First frame of title credits: $FFTC\n"
		"Last frame of title credits: $LFTC\n"
		"First frame of intermission: $FFOI\n"
		"Last frame of intermission: $LFOI\n"
		"First frame of end credits: $FFEC\n"
		"Last frame of end credits: $LFEC\n"
		"First frame of moving credits: $FFMC\n"
		"Last frame of moving credits: $LFMC\n"
		"First frame of ratings band: $FFOB\n"
		"Last frame of ratings band: $LFOB\n"
		"First frame of ratings band (to remove): $FFOB_LINE\n"
		"Last frame of ratings band (to remove): $LFOB_LINE\n"
	);

	dcpomatic::write_cover_sheet(film, "test/data/dcp_digest_test_dcp", "build/test/cover_sheet.txt");
	check_text_file("test/data/cover_sheet.txt", "build/test/cover_sheet.txt");
}

