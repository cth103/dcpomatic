/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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


/** @test  test/stream_test.cc
 *  @brief Some simple tests of FFmpegAudioStream.
 */


#include "lib/ffmpeg_audio_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include <libcxml/cxml.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/test/unit_test.hpp>


using std::list;
using std::pair;


BOOST_AUTO_TEST_CASE (stream_test)
{
	xmlpp::Document doc;
	auto root = doc.create_root_node("FFmpegAudioStream");
	root->add_child("Name")->add_child_text ("hello there world");
	root->add_child("Id")->add_child_text ("4");
	root->add_child("FrameRate")->add_child_text ("44100");
	root->add_child("Channels")->add_child_text ("2");

	/* This is the state file version 5 description of the mapping */

	auto mapping = root->add_child("Mapping");
	mapping->add_child("ContentChannels")->add_child_text ("2");
	{
		/* L -> L */
		auto map = mapping->add_child("Map");
		map->add_child("ContentIndex")->add_child_text ("0");
		map->add_child("DCP")->add_child_text ("0");
	}
	{
		/* L -> C */
		auto map = mapping->add_child("Map");
		map->add_child("ContentIndex")->add_child_text ("0");
		map->add_child("DCP")->add_child_text ("2");
	}
	{
		/* R -> R */
		auto map = mapping->add_child("Map");
		map->add_child("ContentIndex")->add_child_text ("1");
		map->add_child("DCP")->add_child_text ("1");
	}
	{
		/* R -> C */
		auto map = mapping->add_child("Map");
		map->add_child("ContentIndex")->add_child_text ("1");
		map->add_child("DCP")->add_child_text ("2");
	}

	FFmpegAudioStream a (cxml::NodePtr (new cxml::Node (root)), 5);

	BOOST_CHECK_EQUAL (a.identifier(), "4");
	BOOST_CHECK_EQUAL (a.frame_rate(), 44100);
	BOOST_CHECK_EQUAL (a.channels(), 2);
	BOOST_CHECK_EQUAL (a.name, "hello there world");
	BOOST_CHECK_EQUAL (a.mapping().input_channels(), 2);

	BOOST_CHECK_EQUAL (a.mapping().get(0, dcp::Channel::LEFT), 1);
	BOOST_CHECK_EQUAL (a.mapping().get(0, dcp::Channel::RIGHT), 0);
	BOOST_CHECK_EQUAL (a.mapping().get(0, dcp::Channel::CENTRE), 1);
	BOOST_CHECK_EQUAL (a.mapping().get(1, dcp::Channel::LEFT), 0);
	BOOST_CHECK_EQUAL (a.mapping().get(1, dcp::Channel::RIGHT), 1);
	BOOST_CHECK_EQUAL (a.mapping().get(1, dcp::Channel::CENTRE), 1);
}

