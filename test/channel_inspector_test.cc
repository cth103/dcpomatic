/*
    Copyright (C) 2026

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


/** @file  test/channel_inspector_test.cc
 *  @brief Test ChannelInspector class.
 *  @ingroup selfcontained
 */


#include "wx/channel_inspector.h"
#include <boost/test/unit_test.hpp>


static
ChannelInspector::Matrix
zero_matrix()
{
	ChannelInspector::Matrix matrix;

	for (auto& row: matrix.gain) {
		for (auto& gain: row) {
			gain = 0.0f;
		}
	}

	return matrix;
}


BOOST_AUTO_TEST_CASE(channel_inspector_downmix_uses_positive_gain)
{
	ChannelInspector inspector;
	inspector.configure(2, 2, 2);

	auto matrix = zero_matrix();
	matrix.gain[0][0] = 1.0f;
	matrix.gain[1][0] = 0.5f;
	matrix.gain[1][1] = -1.0f;
	inspector.publish(matrix);

	float const mid[] = {
		0.25f, 0.5f,
		-0.5f, -1.0f
	};
	float out[] = {
		99.0f, 99.0f,
		99.0f, 99.0f
	};

	inspector.downmix(mid, out, 2);

	BOOST_CHECK_CLOSE(out[0], 0.5f, 0.001);
	BOOST_CHECK_EQUAL(out[1], 0.0f);
	BOOST_CHECK_CLOSE(out[2], -1.0f, 0.001);
	BOOST_CHECK_EQUAL(out[3], 0.0f);
}


BOOST_AUTO_TEST_CASE(channel_inspector_meter_and_state)
{
	ChannelInspector inspector;
	inspector.configure(2, 2, 2);

	float const mid[] = {
		0.25f, 0.5f,
		-0.5f, -1.0f
	};

	inspector.meter(mid, 2);
	BOOST_CHECK(inspector.peak_dbfs(0) > -6.1f);
	BOOST_CHECK(inspector.peak_dbfs(0) < -6.0f);
	BOOST_CHECK_CLOSE(inspector.peak_dbfs(1), 0.0f, 0.001);

	inspector.set_solo(1, true);
	inspector.set_mute(0, true);
	BOOST_CHECK(inspector.has_solo());
	BOOST_CHECK(inspector.solo(1));
	BOOST_CHECK(inspector.mute(0));

	inspector.clear();
	BOOST_CHECK(!inspector.has_solo());
	BOOST_CHECK(!inspector.solo(1));
	BOOST_CHECK(!inspector.mute(0));
}
