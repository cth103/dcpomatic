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

BOOST_AUTO_TEST_CASE (format_test)
{
	Format::setup_formats ();
	
	Format const * f = Format::from_nickname ("Flat");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->dcp_size().width, 1998);
	BOOST_CHECK_EQUAL (f->dcp_size().height, 1080);
	
	f = Format::from_nickname ("Scope");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->dcp_size().width, 2048);
	BOOST_CHECK_EQUAL (f->dcp_size().height, 858);
}


/* Test VariableFormat-based scaling of content */
BOOST_AUTO_TEST_CASE (scaling_test)
{
	shared_ptr<Film> film (new Film (test_film_dir ("scaling_test").string(), false));

	/* 4:3 ratio */
	film->set_size (libdcp::Size (320, 240));

	/* This format should preserve aspect ratio of the source */
	Format const * format = Format::from_id ("var-185");

	/* We should have enough padding that the result is 4:3,
	   which would be 1440 pixels.
	*/
	BOOST_CHECK_EQUAL (format->dcp_padding (film), (1998 - 1440) / 2);
	
	/* This crops it to 1.291666667 */
	film->set_left_crop (5);
	film->set_right_crop (5);

	/* We should now have enough padding that the result is 1.29166667,
	   which would be 1395 pixels.
	*/
	BOOST_CHECK_EQUAL (format->dcp_padding (film), rint ((1998 - 1395) / 2.0));
}

