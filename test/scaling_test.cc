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

/** @file test/scaling_test.cc
 *  @brief Test scaling and black-padding of images from a still-image source.
 */

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (scaling_test)
{
	shared_ptr<Film> film = new_test_film ("scaling_test");
	film->examine_and_add_content (shared_ptr<Content> (new ImageMagickContent ("test/data/simple_testcard_640x480.png")));
	
}
