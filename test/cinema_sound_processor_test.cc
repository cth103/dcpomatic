/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/datasat_ap2x.h"
#include "lib/dolby_cp750.h"
#include "lib/usl.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (dolby_cp750_test)
{
	DolbyCP750 ap;

	/* No change */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(7, 7), 0, 0.1);
	/* Within 0->4 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(1, 3), 40, 0.1);
	/* Within 0->4 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(3, 1), -40, 0.1);
	/* Within 4->10 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(5, 8), 10, 0.1);
	/* Within 4->10 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(8, 5), -10, 0.1);
	/* Crossing knee, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(3, 6), 20 + 6.66666666666666666, 0.1);
	/* Crossing knee, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(6, 3), -(20 + 6.66666666666666666), 0.1);
}


BOOST_AUTO_TEST_CASE (usl_test)
{
	USL ap;

	/* No change */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(7, 7), 0, 0.1);
	/* Within 0->5.5 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(1, 3), 20, 0.1);
	/* Within 0->5.5 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(3, 1), -20, 0.1);
	/* Within 5.5->10 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(6, 9), 10, 0.1);
	/* Within 5.5->10 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(9, 6), -10, 0.1);
	/* Crossing knee, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(3, 6), (2.5 * 10 + 0.5 * 3.333333333333333333), 0.1);
	/* Crossing knee, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(6, 3), -(2.5 * 10 + 0.5 * 3.333333333333333333), 0.1);
}


BOOST_AUTO_TEST_CASE (datasat_ap2x_test)
{
	DatasatAP2x ap;

	/* No change */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(7, 7), 0, 0.1);
	/* Within 0->3.2 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(0, 2), 40, 0.1);
	/* Within 0->3.2 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(2, 0), -40, 0.1);
	/* Within 3.2->10 range, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(6, 9), 15, 0.1);
	/* Within 3.2->10 range, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(9, 6), -15, 0.1);
	/* Crossing knee, up */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(3, 6), (0.2 * 20 + 2.8 * 5), 0.1);
	/* Crossing knee, down */
	BOOST_CHECK_CLOSE (ap.db_for_fader_change(6, 3), -(0.2 * 20 + 2.8 * 5), 0.1);
}
