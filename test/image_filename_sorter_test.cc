/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/test/unit_test.hpp>

#include "lib/image_filename_sorter.cc"

BOOST_AUTO_TEST_CASE (image_filename_sorter_test)
{
	ImageFilenameSorter x;
	BOOST_ASSERT (x ("abc0000000001", "abc0000000002"));
	BOOST_ASSERT (x ("1", "2"));
	BOOST_ASSERT (x ("1", "0002"));
	BOOST_ASSERT (x ("0001", "2"));
	BOOST_ASSERT (x ("1", "999"));
	BOOST_ASSERT (x ("00057.tif", "00166.tif"));
	BOOST_ASSERT (x ("/my/numeric999/path/00057.tif", "/my/numeric999/path/00166.tif"));

	BOOST_ASSERT (!x ("abc0000000002", "abc0000000001"));
	BOOST_ASSERT (!x ("2", "1"));
	BOOST_ASSERT (!x ("0002", "1"));
	BOOST_ASSERT (!x ("2", "0001"));
	BOOST_ASSERT (!x ("999", "1"));
	BOOST_ASSERT (!x ("/my/numeric999/path/00166.tif", "/my/numeric999/path/00057.tif"));
}
