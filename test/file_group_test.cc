/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <stdint.h>
#include <cstdio>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "lib/file_group.h"

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

	vector<boost::filesystem::path> name;
	boost::filesystem::create_directories ("build/test/file_group_test");
	name.push_back ("build/test/file_group_test/A");
	name.push_back ("build/test/file_group_test/B");
	name.push_back ("build/test/file_group_test/C");
	name.push_back ("build/test/file_group_test/D");

	int base = 0;
	for (int i = 0; i < num_files; ++i) {
		FILE* f = fopen (name[i].string().c_str(), "wb");
		fwrite (data + base, 1, length[i], f);
		fclose (f);
		base += length[i];
	}

	FileGroup fg;
	fg.set_paths (name);
	uint8_t test[65536];

	int pos = 0;

	/* Basic read from 0 */
	BOOST_CHECK_EQUAL (fg.read (test, 64), 64);
	BOOST_CHECK_EQUAL (memcmp (data, test, 64), 0);
	pos += 64;

	/* Another read following the previous */
	BOOST_CHECK_EQUAL (fg.read (test, 4), 4);
	BOOST_CHECK_EQUAL (memcmp (data + pos, test, 4), 0);
	pos += 4;

	/* Read overlapping A and B */
	BOOST_CHECK_EQUAL (fg.read (test, 128), 128);
	BOOST_CHECK_EQUAL (memcmp (data + pos, test, 128), 0);
	pos += 128;

	/* Read overlapping B/C/D and over-reading */
	BOOST_CHECK_EQUAL (fg.read (test, total_length * 3), total_length - pos);
	BOOST_CHECK_EQUAL (memcmp (data + pos, test, total_length - pos), 0);

	/* Bad seek */
	BOOST_CHECK_EQUAL (fg.seek (total_length * 2, SEEK_SET), -1);

	/* SEEK_SET */
	BOOST_CHECK_EQUAL (fg.seek (999, SEEK_SET), 999);
	BOOST_CHECK_EQUAL (fg.read (test, 64), 64);
	BOOST_CHECK_EQUAL (memcmp (data + 999, test, 64), 0);

	/* SEEK_CUR */
	BOOST_CHECK_EQUAL (fg.seek (42, SEEK_CUR), 999 + 64 + 42);
	BOOST_CHECK_EQUAL (fg.read (test, 64), 64);
	BOOST_CHECK_EQUAL (memcmp (data + 999 + 64 + 42, test, 64), 0);

	/* SEEK_END */
	BOOST_CHECK_EQUAL (fg.seek (1077, SEEK_END), total_length - 1077);
	BOOST_CHECK_EQUAL (fg.read (test, 256), 256);
	BOOST_CHECK_EQUAL (memcmp (data + total_length - 1077, test, 256), 0);
}
