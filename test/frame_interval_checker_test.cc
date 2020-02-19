/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "lib/frame_interval_checker.h"
#include <boost/test/unit_test.hpp>

/** Test of 2D-ish frame timings */
BOOST_AUTO_TEST_CASE (frame_interval_checker_test1)
{
	FrameIntervalChecker checker;
	ContentTime t(3888);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4012);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3776);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3779);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4010);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4085);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4085);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4012);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3776);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3779);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4010);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4085);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4085);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::PROBABLY_NOT_3D);
}

/** Test of 3D-ish frame timings */
BOOST_AUTO_TEST_CASE (frame_interval_checker_test2)
{
	FrameIntervalChecker checker;
	ContentTime t(3888);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(0);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(0);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3776);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(50);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4010);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(2);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4011);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(0);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4000);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(0);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(3776);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(50);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4010);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(2);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::AGAIN);
	t += ContentTime(4011);
	checker.feed (t, 24);
	BOOST_CHECK (checker.guess() == FrameIntervalChecker::PROBABLY_3D);
}


