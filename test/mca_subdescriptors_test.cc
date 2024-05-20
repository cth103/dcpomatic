/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/audio_content.h"
#include "lib/constants.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <libcxml/cxml.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::string;
using std::vector;


static
void
test_descriptors(int mxf_channels, vector<dcp::Channel> active_channels, vector<string> mca_tag_symbols, string group_name)
{
	auto content = content_factory("test/data/flat_red.png");
	for (auto i = 0; i < mxf_channels; ++i) {
		content.push_back(content_factory("test/data/C.wav").front());
	}
	auto film = new_test_film("mca_subdescriptors_written_correctly", content);
	film->set_interop(false);
	film->set_audio_channels(mxf_channels);

	int N = 1;
	for (auto channel: active_channels) {
		auto mapping = AudioMapping(1, MAX_DCP_AUDIO_CHANNELS);
		mapping.set(0, channel, 1);
		content[N]->audio->set_mapping(mapping);
		++N;
	}

	make_and_verify_dcp(film);

	cxml::Document check("CompositionPlaylist", find_file(film->dir(film->dcp_name()), "cpl_"));
	vector<string> cpl_mca_tag_symbols;

	auto mca_sub_descriptors = check.node_child("ReelList")->node_child("Reel")->node_child("AssetList")->node_child("CompositionMetadataAsset")->node_child("MCASubDescriptors");

	for (auto node: mca_sub_descriptors->node_children("AudioChannelLabelSubDescriptor")) {
		cpl_mca_tag_symbols.push_back(node->string_child("MCATagSymbol"));
	}

	auto const cpl_group_name = mca_sub_descriptors->node_child("SoundfieldGroupLabelSubDescriptor")->string_child("MCATagSymbol");

	BOOST_CHECK(cpl_mca_tag_symbols == mca_tag_symbols);
	BOOST_CHECK(cpl_group_name == group_name);
}


/* This seems like an impossible case but let's check it anyway */
BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_mono_in_2_channel)
{
	test_descriptors(2, { dcp::Channel::CENTRE }, { "chL", "chR" }, "sg51");
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_mono_in_6_channel)
{
	test_descriptors(6, { dcp::Channel::CENTRE }, { "chL", "chR", "chC", "chLFE", "chLs", "chRs" }, "sg51");
}


/* If we only have two channels in the MXF we shouldn't see any extra descriptors */
BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_stereo_in_2_channel)
{
	test_descriptors(2, { dcp::Channel::LEFT, dcp::Channel::RIGHT }, { "chL", "chR" }, "sg51");
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_stereo_in_6_channel)
{
	test_descriptors(6, { dcp::Channel::LEFT, dcp::Channel::RIGHT }, { "chL", "chR", "chC", "chLFE", "chLs", "chRs" }, "sg51");
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_51)
{
	test_descriptors(6,
		{
			dcp::Channel::LEFT,
			dcp::Channel::RIGHT,
			dcp::Channel::CENTRE,
			dcp::Channel::LFE,
			dcp::Channel::LS,
			dcp::Channel::RS,
		},
		{ "chL", "chR", "chC", "chLFE", "chLs", "chRs" },
		"sg51"
		);
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_51_with_hi_vi)
{
	test_descriptors(8,
		{
			dcp::Channel::LEFT,
			dcp::Channel::RIGHT,
			dcp::Channel::CENTRE,
			dcp::Channel::LFE,
			dcp::Channel::LS,
			dcp::Channel::RS,
			dcp::Channel::HI,
			dcp::Channel::VI,
		},
		{ "chL", "chR", "chC", "chLFE", "chLs", "chRs", "chHI", "chVIN" },
		"sg51"
		);
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_71)
{
	test_descriptors(16,
		{
			dcp::Channel::LEFT,
			dcp::Channel::RIGHT,
			dcp::Channel::CENTRE,
			dcp::Channel::LFE,
			dcp::Channel::LS,
			dcp::Channel::RS,
			dcp::Channel::BSL,
			dcp::Channel::BSR,
		},
		{ "chL", "chR", "chC", "chLFE", "chLss", "chRss", "chLrs", "chRrs" },
		"sg71"
		);
}


BOOST_AUTO_TEST_CASE(mca_subdescriptors_written_correctly_71_with_hi_vi)
{
	test_descriptors(16,
		{
			dcp::Channel::LEFT,
			dcp::Channel::RIGHT,
			dcp::Channel::CENTRE,
			dcp::Channel::LFE,
			dcp::Channel::LS,
			dcp::Channel::RS,
			dcp::Channel::HI,
			dcp::Channel::VI,
			dcp::Channel::BSL,
			dcp::Channel::BSR,
		},
		{ "chL", "chR", "chC", "chLFE", "chLss", "chRss", "chHI", "chVIN", "chLrs", "chRrs" },
		"sg71"
		);
}
