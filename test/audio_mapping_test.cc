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


#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <dcp/types.h>
#include <fmt/format.h>
#include <algorithm>


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


/** Changing a content's audio mapping is signalled, and mapped_audio_channels() follows it, which is what
 *  the View > Audio menu's HI and VI choices depend on.
 */
BOOST_AUTO_TEST_CASE(audio_mapping_change_signalled_test)
{
	auto content = content_factory("test/data/L.wav")[0];
	auto film = new_test_film("audio_mapping_change_signalled_test", { content });
	film->set_audio_channels(16);

	int signalled = 0;
	film->ContentChange.connect([&signalled](ChangeType type, int property, bool) {
		if (type == ChangeType::DONE && property == AudioContentProperty::STREAMS) {
			++signalled;
		}
	});

	auto has = [film](dcp::Channel channel) {
		auto const mapped = film->mapped_audio_channels();
		return std::find(mapped.begin(), mapped.end(), static_cast<int>(channel)) != mapped.end();
	};

	AudioMapping map(1, MAX_DCP_AUDIO_CHANNELS);
	map.make_zero();
	map.set(0, dcp::Channel::CENTRE, 1);
	content->audio->set_mapping(map);
	BOOST_CHECK_EQUAL(signalled, 1);
	BOOST_CHECK(!has(dcp::Channel::HI));

	map.set(0, dcp::Channel::HI, 1);
	content->audio->set_mapping(map);
	BOOST_CHECK_EQUAL(signalled, 2);
	BOOST_CHECK(has(dcp::Channel::HI));
	BOOST_CHECK(!has(dcp::Channel::VI));

	map.set(0, dcp::Channel::HI, 0);
	content->audio->set_mapping(map);
	BOOST_CHECK_EQUAL(signalled, 3);
	BOOST_CHECK(!has(dcp::Channel::HI));
}


/** Listening to one channel sends only that channel: to both speakers in stereo, to the centre otherwise */
BOOST_AUTO_TEST_CASE(audio_mapping_listen_to_test)
{
	auto check = [](AudioMapping const& mapping, dcp::Channel channel, std::vector<int> outputs) {
		for (int input = 0; input < mapping.input_channels(); ++input) {
			for (int output = 0; output < mapping.output_channels(); ++output) {
				auto const expected = input == static_cast<int>(channel) && std::find(outputs.begin(), outputs.end(), output) != outputs.end();
				BOOST_CHECK_MESSAGE(mapping.get(input, output) == (expected ? 1 : 0), "input " << input << " output " << output);
			}
		}
	};

	auto stereo = listen_to_mapping(dcp::Channel::HI, 2);
	BOOST_CHECK_EQUAL(stereo.input_channels(), MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL(stereo.output_channels(), 2);
	check(stereo, dcp::Channel::HI, { 0, 1 });

	check(listen_to_mapping(dcp::Channel::VI, 6), dcp::Channel::VI, { static_cast<int>(dcp::Channel::CENTRE) });
	check(listen_to_mapping(dcp::Channel::HI, 16), dcp::Channel::HI, { static_cast<int>(dcp::Channel::CENTRE) });
	check(listen_to_mapping(dcp::Channel::VI, 1), dcp::Channel::VI, { 0 });
}
