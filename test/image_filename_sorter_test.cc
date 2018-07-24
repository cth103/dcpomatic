/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  test/image_filename_sorter_test.cc
 *  @brief Test ImageFilenameSorter
 *  @ingroup selfcontained
 */

#include "lib/image_filename_sorter.h"
#include "lib/compose.hpp"
#include <boost/test/unit_test.hpp>

using std::random_shuffle;
using std::sort;
using std::vector;

BOOST_AUTO_TEST_CASE (image_filename_sorter_test1)
{
	ImageFilenameSorter x;
	BOOST_CHECK (x ("abc0000000001", "abc0000000002"));
	BOOST_CHECK (x ("1", "2"));
	BOOST_CHECK (x ("1", "0002"));
	BOOST_CHECK (x ("0001", "2"));
	BOOST_CHECK (x ("1", "999"));
	BOOST_CHECK (x ("00057.tif", "00166.tif"));
	BOOST_CHECK (x ("/my/numeric999/path/00057.tif", "/my/numeric999/path/00166.tif"));
	BOOST_CHECK (x ("1_01.tif", "1_02.tif"));
	BOOST_CHECK (x ("EWS_DCP_092815_000000.j2c", "EWS_DCP_092815_000001.j2c"));
	BOOST_CHECK (x ("ap_trlr_178_uhd_bt1886_txt_e5c1_033115.86352.dpx", "ap_trlr_178_uhd_bt1886_txt_e5c1_033115.86353.dpx"));

	BOOST_CHECK (!x ("abc0000000002", "abc0000000001"));
	BOOST_CHECK (!x ("2", "1"));
	BOOST_CHECK (!x ("0002", "1"));
	BOOST_CHECK (!x ("2", "0001"));
	BOOST_CHECK (!x ("999", "1"));
	BOOST_CHECK (!x ("/my/numeric999/path/00166.tif", "/my/numeric999/path/00057.tif"));
	BOOST_CHECK (!x ("1_02.tif", "1_01.tif"));
	BOOST_CHECK (!x ("EWS_DCP_092815_000000.j2c", "EWS_DCP_092815_000000.j2c"));
	BOOST_CHECK (!x ("EWS_DCP_092815_000100.j2c", "EWS_DCP_092815_000000.j2c"));
	BOOST_CHECK (!x ("ap_trlr_178_uhd_bt1886_txt_e5c1_033115.86353.dpx", "ap_trlr_178_uhd_bt1886_txt_e5c1_033115.86352.dpx"));
}

/** Test a sort of a lot of paths.  Mostly useful for profiling. */
BOOST_AUTO_TEST_CASE (image_filename_sorter_test2)
{
	vector<boost::filesystem::path> paths;
	for (int i = 0; i < 100000; ++i) {
		paths.push_back(String::compose("some.filename.with.%1.number.tiff", i));
	}
	random_shuffle (paths.begin(), paths.end());
	sort (paths.begin(), paths.end(), ImageFilenameSorter());
	for (int i = 0; i < 100000; ++i) {
		BOOST_CHECK_EQUAL(paths[i].string(), String::compose("some.filename.with.%1.number.tiff", i));
	}
}
