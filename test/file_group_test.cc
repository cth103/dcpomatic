/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/file_group_test.cc
 *  @brief Test FileGroup class.
 *  @ingroup selfcontained
 */


#include "lib/file_group.h"
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <stdint.h>
#include <cstdio>


using std::vector;


BOOST_AUTO_TEST_CASE (file_group_test)
{
	/* Random data; must be big enough for all the files */
	uint8_t data[65536];
	for (int i = 0; i < 65536; ++i) {
		data[i] = rand() & 0xff;
	}

	int const num_files = 4;

	int length[] = {
		99,
		18941,
		33110,
		42
	};

	int total_length = 0;
	for (int i = 0; i < num_files; ++i) {
		total_length += length[i];
	}

	boost::filesystem::create_directories ("build/test/file_group_test");
	vector<boost::filesystem::path> name = {
		"build/test/file_group_test/A",
		"build/test/file_group_test/B",
		"build/test/file_group_test/C",
		"build/test/file_group_test/D"
	};

	int base = 0;
	for (int i = 0; i < num_files; ++i) {
		auto f = fopen (name[i].string().c_str(), "wb");
		fwrite (data + base, 1, length[i], f);
		fclose (f);
		base += length[i];
	}

	FileGroup fg (name);
	uint8_t test[65536];

	int pos = 0;

	/* Basic read from 0 */
	BOOST_CHECK_EQUAL(fg.read(test, 64).bytes_read, 64);
	BOOST_CHECK_EQUAL (memcmp(data, test, 64), 0);
	pos += 64;

	/* Another read following the previous */
	BOOST_CHECK_EQUAL(fg.read(test, 4).bytes_read, 4);
	BOOST_CHECK_EQUAL (memcmp(data + pos, test, 4), 0);
	pos += 4;

	/* Read overlapping A and B */
	BOOST_CHECK_EQUAL(fg.read(test, 128).bytes_read, 128);
	BOOST_CHECK_EQUAL (memcmp(data + pos, test, 128), 0);
	pos += 128;

	/* Read overlapping B/C/D and over-reading by a lot */
	BOOST_CHECK_EQUAL(fg.read(test, total_length * 3).bytes_read, total_length - pos);
	BOOST_CHECK_EQUAL (memcmp(data + pos, test, total_length - pos), 0);

	/* Over-read by a little */
	BOOST_CHECK_EQUAL (fg.seek(0, SEEK_SET), 0);
	BOOST_CHECK_EQUAL(fg.read(test, total_length).bytes_read, total_length);
	BOOST_CHECK_EQUAL(fg.read(test, 1).bytes_read, 0);

	/* Seeking off the end of the file should not give an error */
	BOOST_CHECK_EQUAL (fg.seek(total_length * 2, SEEK_SET), total_length * 2);
	/* and attempting to read should return nothing and EOF */
	auto result = fg.read(test, 64);
	BOOST_CHECK_EQUAL(result.bytes_read, 0);
	BOOST_CHECK(result.eof);
	/* but the requested seek should be remembered, so if we now go back (relatively) */
	BOOST_CHECK_EQUAL (fg.seek(-total_length * 2, SEEK_CUR), 0);
	/* we should be at the start again */
	BOOST_CHECK_EQUAL(fg.read(test, 64).bytes_read, 64);
	BOOST_CHECK_EQUAL (memcmp(data, test, 64), 0);

	/* SEEK_SET */
	BOOST_CHECK_EQUAL (fg.seek(999, SEEK_SET), 999);
	BOOST_CHECK_EQUAL(fg.read(test, 64).bytes_read, 64);
	BOOST_CHECK_EQUAL (memcmp(data + 999, test, 64), 0);

	/* SEEK_CUR */
	BOOST_CHECK_EQUAL (fg.seek(42, SEEK_CUR), 999 + 64 + 42);
	BOOST_CHECK_EQUAL(fg.read(test, 64).bytes_read, 64);
	BOOST_CHECK_EQUAL (memcmp(data + 999 + 64 + 42, test, 64), 0);

	/* SEEK_END */
	BOOST_CHECK_EQUAL (fg.seek(1077, SEEK_END), total_length - 1077);
	BOOST_CHECK_EQUAL(fg.read(test, 256).bytes_read, 256);
	BOOST_CHECK_EQUAL (memcmp(data + total_length - 1077, test, 256), 0);
}
