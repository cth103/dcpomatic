/*
    Copyright (C) 2016-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/audio_ring_buffers.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::cout;
using boost::shared_ptr;

#define CANARY 9999

/* XXX: these tests don't check the timestamping in AudioRingBuffers */

/** Basic tests fetching the same number of channels as went in */
BOOST_AUTO_TEST_CASE (audio_ring_buffers_test1)
{
	AudioRingBuffers rb;

	/* Should start off empty */
	BOOST_CHECK_EQUAL (rb.size(), 0);

	/* Getting some data should give an underrun and write zeros */
	float buffer[256 * 6];
	buffer[240 * 6] = CANARY;
	BOOST_CHECK (!rb.get(buffer, 6, 240));
	for (int i = 0; i < 240 * 6; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i], 0);
	}
	BOOST_CHECK_EQUAL (buffer[240 * 6], CANARY);

	/* clear() should give the same result */
	rb.clear ();
	BOOST_CHECK_EQUAL (rb.size(), 0);
	buffer[240 * 6] = CANARY;
	BOOST_CHECK (rb.get(buffer, 6, 240) == boost::optional<DCPTime>());
	for (int i = 0; i < 240 * 6; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i], 0);
	}
	BOOST_CHECK_EQUAL (buffer[240 * 6], CANARY);

	/* Put some data in */
	shared_ptr<AudioBuffers> data (new AudioBuffers (6, 91));
	int value = 0;
	for (int i = 0; i < 91; ++i) {
		for (int j = 0; j < 6; ++j) {
			data->data(j)[i] = value++;
		}
	}
	rb.put (data, DCPTime());
	BOOST_CHECK_EQUAL (rb.size(), 91);

	/* Get part of it out */
	buffer[40 * 6] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 6, 40) == DCPTime());
	int check = 0;
	for (int i = 0; i < 40 * 6; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i], check++);
	}
	BOOST_CHECK_EQUAL (buffer[40 * 6], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 51);

	/* Get the rest */
	buffer[51 * 6] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 6, 51) == DCPTime::from_frames(40, 48000));
	for (int i = 0; i < 51 * 6; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i], check++);
	}
	BOOST_CHECK_EQUAL (buffer[51 * 6], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 0);

	/* Now there should be an underrun */
	buffer[240 * 6] = CANARY;
	BOOST_CHECK (!rb.get(buffer, 6, 240));
	BOOST_CHECK_EQUAL (buffer[240 * 6], CANARY);
}

/** Similar tests but fetching more channels than were put in */
BOOST_AUTO_TEST_CASE (audio_ring_buffers_test2)
{
	AudioRingBuffers rb;

	/* Put some data in */
	shared_ptr<AudioBuffers> data (new AudioBuffers (2, 91));
	int value = 0;
	for (int i = 0; i < 91; ++i) {
		for (int j = 0; j < 2; ++j) {
			data->data(j)[i] = value++;
		}
	}
	rb.put (data, DCPTime());
	BOOST_CHECK_EQUAL (rb.size(), 91);

	/* Get part of it out */
	float buffer[256 * 6];
	buffer[40 * 6] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 6, 40) == DCPTime());
	int check = 0;
	for (int i = 0; i < 40; ++i) {
		for (int j = 0; j < 2; ++j) {
			BOOST_REQUIRE_EQUAL (buffer[i * 6 + j], check++);
		}
		for (int j = 2; j < 6; ++j) {
			BOOST_REQUIRE_EQUAL (buffer[i * 6 + j], 0);
		}
	}
	BOOST_CHECK_EQUAL (buffer[40 * 6], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 51);

	/* Get the rest */
	buffer[51 * 6] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 6, 51) == DCPTime::from_frames(40, 48000));
	for (int i = 0; i < 51; ++i) {
		for (int j = 0; j < 2; ++j) {
			BOOST_REQUIRE_EQUAL (buffer[i * 6 + j], check++);
		}
		for (int j = 2; j < 6; ++j) {
			BOOST_REQUIRE_EQUAL (buffer[i * 6 + j], 0);
		}
	}
	BOOST_CHECK_EQUAL (buffer[51 * 6], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 0);

	/* Now there should be an underrun */
	buffer[240 * 6] = CANARY;
	BOOST_CHECK (!rb.get(buffer, 6, 240));
	BOOST_CHECK_EQUAL (buffer[240 * 6], CANARY);
}

/** Similar tests but fetching fewer channels than were put in */
BOOST_AUTO_TEST_CASE (audio_ring_buffers_test3)
{
	AudioRingBuffers rb;

	/* Put some data in */
	shared_ptr<AudioBuffers> data (new AudioBuffers (6, 91));
	int value = 0;
	for (int i = 0; i < 91; ++i) {
		for (int j = 0; j < 6; ++j) {
			data->data(j)[i] = value++;
		}
	}
	rb.put (data, DCPTime ());
	BOOST_CHECK_EQUAL (rb.size(), 91);

	/* Get part of it out */
	float buffer[256 * 6];
	buffer[40 * 2] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 2, 40) == DCPTime());
	int check = 0;
	for (int i = 0; i < 40; ++i) {
		for (int j = 0; j < 2; ++j) {
			BOOST_REQUIRE_EQUAL (buffer[i * 2 + j], check++);
		}
		check += 4;
	}
	BOOST_CHECK_EQUAL (buffer[40 * 2], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 51);

	/* Get the rest */
	buffer[51 * 2] = CANARY;
	BOOST_CHECK (*rb.get(buffer, 2, 51) == DCPTime::from_frames(40, 48000));
	for (int i = 0; i < 51; ++i) {
		for (int j = 0; j < 2; ++j)  {
			BOOST_REQUIRE_EQUAL (buffer[i * 2 + j], check++);
		}
		check += 4;
	}
	BOOST_CHECK_EQUAL (buffer[51 * 2], CANARY);
	BOOST_CHECK_EQUAL (rb.size(), 0);

	/* Now there should be an underrun */
	buffer[240 * 2] = CANARY;
	BOOST_CHECK (!rb.get(buffer, 2, 240));
	BOOST_CHECK_EQUAL (buffer[240 * 2], CANARY);
}
