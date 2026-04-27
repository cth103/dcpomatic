/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/audio_mapping_test.cc
 *  @brief Test AudioMapping class.
 *  @ingroup selfcontained
 */


#include "lib/audio_mapping.h"
#include "lib/constants.h"
#include <boost/test/unit_test.hpp>
#include <fmt/format.h>


using std::list;
using std::string;
using boost::optional;


BOOST_AUTO_TEST_CASE(audio_mapping_test)
{
	AudioMapping none;
	BOOST_CHECK_EQUAL(none.input_channels(), 0);

	AudioMapping four(4, MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL(four.input_channels(), 4);

	four.set(0, 1, 1);

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < MAX_DCP_AUDIO_CHANNELS; ++j) {
			BOOST_CHECK_EQUAL(four.get(i, j), (i == 0 && j == 1) ? 1 : 0);
		}
	}

	auto mapped = four.mapped_output_channels();
	BOOST_CHECK_EQUAL(mapped.size(), 1U);
	BOOST_CHECK_EQUAL(mapped.front(), 1);

	four.make_zero();

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < MAX_DCP_AUDIO_CHANNELS; ++j) {
			BOOST_CHECK_EQUAL(four.get(i, j), 0);
		}
	}
}


static void
guess_check(boost::filesystem::path filename, dcp::Channel output_channel)
{
	AudioMapping m(1, 12);
	m.make_default(0, filename);
	for (int i = 0; i < 12; ++i) {
		BOOST_TEST_INFO(fmt::format("{} channel {}", filename.string(), i));
		BOOST_CHECK_CLOSE(m.get(0, i), i == static_cast<int>(output_channel) ? 1 : 0, 0.01);
	}
}


BOOST_AUTO_TEST_CASE(audio_mapping_guess_test)
{
	guess_check("stuff_L_nonsense.wav", dcp::Channel::LEFT);
	guess_check("stuff_nonsense.wav", dcp::Channel::CENTRE);
	guess_check("fred_R.wav", dcp::Channel::RIGHT);
	guess_check("jim_C_sheila.aiff", dcp::Channel::CENTRE);
	guess_check("things_Lfe_and.wav", dcp::Channel::LFE);
	guess_check("weeee_Ls.aiff", dcp::Channel::LS);
	guess_check("try_Rs-it.wav", dcp::Channel::RS);

	/* PT-style */
	guess_check("things_LFE.wav", dcp::Channel::LFE);
	guess_check("ptish_Lsr_abc.wav", dcp::Channel::BSL);
	guess_check("ptish_Rsr_abc.wav", dcp::Channel::BSR);
	guess_check("more_Lss_s.wav", dcp::Channel::LS);
	guess_check("other_Rss.aiff", dcp::Channel::RS);

	/* Only the filename should be taken into account */
	guess_check("-Lfe-/foo_L.wav", dcp::Channel::LEFT);

	/* Dolby-style */
	guess_check("jake-Lrs-good.wav", dcp::Channel::BSL);
	guess_check("elwood-Rrs-good.wav", dcp::Channel::BSR);
}


BOOST_AUTO_TEST_CASE(audio_mapping_take_from_larger)
{
	AudioMapping A(4, 9);
	AudioMapping B(2, 3);

	A.set(0, 0, 4);
	A.set(1, 0, 8);
	A.set(0, 1, 3);
	A.set(1, 1, 6);
	A.set(0, 2, 1);
	A.set(1, 2, 9);

	B.take_from(A);

	BOOST_CHECK_CLOSE(B.get(0, 0), 4, 0.01);
	BOOST_CHECK_CLOSE(B.get(1, 0), 8, 0.01);
	BOOST_CHECK_CLOSE(B.get(0, 1), 3, 0.01);
	BOOST_CHECK_CLOSE(B.get(1, 1), 6, 0.01);
	BOOST_CHECK_CLOSE(B.get(0, 2), 1, 0.01);
	BOOST_CHECK_CLOSE(B.get(1, 2), 9, 0.01);
}


BOOST_AUTO_TEST_CASE(audio_mapping_take_from_smaller)
{
	AudioMapping A(4, 9);
	AudioMapping B(2, 3);

	B.set(0, 0, 4);
	B.set(1, 0, 8);
	B.set(0, 1, 3);
	B.set(1, 1, 6);
	B.set(0, 2, 1);
	B.set(1, 2, 9);

	A.take_from(B);

	BOOST_CHECK_CLOSE(A.get(0, 0), 4, 0.01);
	BOOST_CHECK_CLOSE(A.get(1, 0), 8, 0.01);
	BOOST_CHECK_CLOSE(A.get(0, 1), 3, 0.01);
	BOOST_CHECK_CLOSE(A.get(1, 1), 6, 0.01);
	BOOST_CHECK_CLOSE(A.get(0, 2), 1, 0.01);
	BOOST_CHECK_CLOSE(A.get(1, 2), 9, 0.01);
}


BOOST_AUTO_TEST_CASE(audio_mapping_test_mapped_one_to_one)
{
	AudioMapping A(6, 6);
	A.make_default(nullptr);
	BOOST_CHECK(A.mapped_one_to_one());

	AudioMapping B(6, 4);
	B.make_default(nullptr);
	BOOST_CHECK(!B.mapped_one_to_one());

	AudioMapping C(6, 10);
	C.make_default(nullptr);
	BOOST_CHECK(C.mapped_one_to_one());

	AudioMapping D(6, 10);
	D.make_default(nullptr);
	D.set(1, 2, 0.5);
	BOOST_CHECK(!D.mapped_one_to_one());

	AudioMapping E(4, 4);
	E.make_default(nullptr);
	E.set(2, 2, 0.5);
	BOOST_CHECK(!E.mapped_one_to_one());
}

