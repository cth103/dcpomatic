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
	cxml::add_text_child(root, "Name", "hello there world");
	cxml::add_text_child(root, "Id", "4");
	cxml::add_text_child(root, "FrameRate", "44100");
	cxml::add_text_child(root, "Channels", "2");

	/* This is the state file version 5 description of the mapping */

	auto mapping = cxml::add_child(root, "Mapping");
	cxml::add_text_child(mapping, "ContentChannels", "2");
	{
		/* L -> L */
		auto map = cxml::add_child(mapping, "Map");
		cxml::add_text_child(map, "ContentIndex", "0");
		cxml::add_text_child(map, "DCP", "0");
	}
	{
		/* L -> C */
		auto map = cxml::add_child(mapping, "Map");
		cxml::add_text_child(map, "ContentIndex", "0");
		cxml::add_text_child(map, "DCP", "2");
	}
	{
		/* R -> R */
		auto map = cxml::add_child(mapping, "Map");
		cxml::add_text_child(map, "ContentIndex", "1");
		cxml::add_text_child(map, "DCP", "1");
	}
	{
		/* R -> C */
		auto map = cxml::add_child(mapping, "Map");
		cxml::add_text_child(map, "ContentIndex", "1");
		cxml::add_text_child(map, "DCP", "2");
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

