/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  test/dcdm_audio_layout_test.cc
 *  @brief Tests of the DCDM audio layout helpers.
 *  @ingroup feature
 */


#include "lib/audio_buffers.h"
#include "lib/dcdm_audio_layout.h"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>
#include <set>


using std::make_shared;
using std::set;


BOOST_AUTO_TEST_CASE(dcdm_layout_51_test)
{
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::RIGHT }) == DCDMLayout::FIVE_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE, dcp::Channel::LS, dcp::Channel::RS }) == DCDMLayout::FIVE_ONE);
	BOOST_CHECK(dcdm_layout({}) == DCDMLayout::FIVE_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_layout_71_test)
{
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::BSL }) == DCDMLayout::SEVEN_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::BSR }) == DCDMLayout::SEVEN_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_layout_extras_do_not_change_layout_test)
{
	/* HI, VI, Lc, Rc, sign language and non-programme channels do not change the layout, and are not refused */
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::HI, dcp::Channel::VI, dcp::Channel::LC, dcp::Channel::RC, dcp::Channel::SIGN_LANGUAGE }) == DCDMLayout::FIVE_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::BSL, dcp::Channel::HI }) == DCDMLayout::SEVEN_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::SYNC_SIGNAL }) == DCDMLayout::FIVE_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_extra_channels_test)
{
	BOOST_CHECK(dcdm_extra_channels({ dcp::Channel::LEFT, dcp::Channel::BSL, dcp::Channel::SYNC_SIGNAL, dcp::Channel::LC, dcp::Channel::RC }).empty());

	auto const extras = dcdm_extra_channels({
		dcp::Channel::SIGN_LANGUAGE, dcp::Channel::RC, dcp::Channel::VI, dcp::Channel::LEFT, dcp::Channel::LC, dcp::Channel::HI
	});
	/* Lc and Rc are left out: DCP-o-matic always writes them silent in a DCP */
	BOOST_REQUIRE_EQUAL(extras.size(), 3U);
	BOOST_CHECK(extras[0].first == dcp::Channel::HI);
	BOOST_CHECK_EQUAL(extras[0].second, "HI");
	BOOST_CHECK(extras[1].first == dcp::Channel::VI);
	BOOST_CHECK_EQUAL(extras[1].second, "VI");
	BOOST_CHECK(extras[2].first == dcp::Channel::SIGN_LANGUAGE);
	BOOST_CHECK_EQUAL(extras[2].second, "SL");

	auto const only_vi = dcdm_extra_channels({ dcp::Channel::VI });
	BOOST_REQUIRE_EQUAL(only_vi.size(), 1U);
	BOOST_CHECK(only_vi[0].first == dcp::Channel::VI);
}


BOOST_AUTO_TEST_CASE(dcdm_pick_channels_with_extras_test)
{
	auto in = make_shared<AudioBuffers>(16, 2);
	for (int c = 0; c < 16; ++c) {
		for (int i = 0; i < 2; ++i) {
			in->data(c)[i] = c / 100.0f;
		}
	}
	auto channels = dcdm_channels(DCDMLayout::FIVE_ONE);
	channels.push_back(dcp::Channel::HI);
	channels.push_back(dcp::Channel::SIGN_LANGUAGE);
	auto out = dcdm_pick_channels(in, channels);
	BOOST_REQUIRE_EQUAL(out->channels(), 8);
	int const expected_source[] = { 0, 1, 2, 3, 4, 5, 6, 14 };
	for (int c = 0; c < 8; ++c) {
		BOOST_CHECK_EQUAL(out->data(c)[0], expected_source[c] / 100.0f);
	}
}


BOOST_AUTO_TEST_CASE(dcdm_labels_test)
{
	auto const five = dcdm_channel_labels(DCDMLayout::FIVE_ONE);
	BOOST_REQUIRE_EQUAL(five.size(), 6U);
	BOOST_CHECK_EQUAL(five[0], "L");
	BOOST_CHECK_EQUAL(five[1], "R");
	BOOST_CHECK_EQUAL(five[2], "C");
	BOOST_CHECK_EQUAL(five[3], "LFE");
	BOOST_CHECK_EQUAL(five[4], "Ls");
	BOOST_CHECK_EQUAL(five[5], "Rs");

	auto const seven = dcdm_channel_labels(DCDMLayout::SEVEN_ONE);
	BOOST_REQUIRE_EQUAL(seven.size(), 8U);
	BOOST_CHECK_EQUAL(seven[6], "Lrs");
	BOOST_CHECK_EQUAL(seven[7], "Rrs");
}


BOOST_AUTO_TEST_CASE(dcdm_layout_channels_stereo_test)
{
	auto in = make_shared<AudioBuffers>(2, 4);
	for (int i = 0; i < 4; ++i) {
		in->data(0)[i] = 0.25f;
		in->data(1)[i] = -0.5f;
	}
	auto out = dcdm_layout_channels(in, DCDMLayout::FIVE_ONE);
	BOOST_REQUIRE_EQUAL(out->channels(), 6);
	BOOST_REQUIRE_EQUAL(out->frames(), 4);
	for (int i = 0; i < 4; ++i) {
		BOOST_CHECK_EQUAL(out->data(0)[i], 0.25f);
		BOOST_CHECK_EQUAL(out->data(1)[i], -0.5f);
		for (int c = 2; c < 6; ++c) {
			BOOST_CHECK_EQUAL(out->data(c)[i], 0.0f);
		}
	}
}


BOOST_AUTO_TEST_CASE(dcdm_layout_channels_71_test)
{
	/* 12 DCP channels: value is channel index / 100 */
	auto in = make_shared<AudioBuffers>(12, 2);
	for (int c = 0; c < 12; ++c) {
		for (int i = 0; i < 2; ++i) {
			in->data(c)[i] = c / 100.0f;
		}
	}
	auto out = dcdm_layout_channels(in, DCDMLayout::SEVEN_ONE);
	BOOST_REQUIRE_EQUAL(out->channels(), 8);
	int const expected_source[] = { 0, 1, 2, 3, 4, 5, 10, 11 };
	for (int c = 0; c < 8; ++c) {
		BOOST_CHECK_EQUAL(out->data(c)[0], expected_source[c] / 100.0f);
	}
}
