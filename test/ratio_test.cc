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

#include <iostream>
#include <boost/test/unit_test.hpp>
#include <libdcp/util.h>
#include "lib/ratio.h"
#include "lib/util.h"

using std::ostream;

namespace libdcp {
	
ostream&
operator<< (ostream& s, libdcp::Size const & t)
{
	s << t.width << "x" << t.height;
	return s;
}

}

BOOST_AUTO_TEST_CASE (ratio_test)
{
	Ratio::setup_ratios ();

	Ratio const * r = Ratio::from_id ("119");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1290, 1080));

	r = Ratio::from_id ("133");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1440, 1080));

	r = Ratio::from_id ("137");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1480, 1080));

	r = Ratio::from_id ("138");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1485, 1080));

	r = Ratio::from_id ("166");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1800, 1080));

	r = Ratio::from_id ("178");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1920, 1080));

	r = Ratio::from_id ("185");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (1998, 1080));

	r = Ratio::from_id ("239");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (2048, 858));

	r = Ratio::from_id ("full-frame");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), libdcp::Size (2048, 1080)), libdcp::Size (2048, 1080));
}

