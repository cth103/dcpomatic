/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/test/unit_test.hpp>
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>
#include "lib/ffmpeg_content.h"
#include "lib/film.h"

using std::pair;
using std::list;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (stream_test)
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("FFmpegAudioStream");
	root->add_child("Name")->add_child_text ("hello there world");
	root->add_child("Id")->add_child_text ("4");
	root->add_child("FrameRate")->add_child_text ("44100");
	root->add_child("Channels")->add_child_text ("2");

	/* This is the state file version 5 description of the mapping */
	
	xmlpp::Element* mapping = root->add_child("Mapping");
	mapping->add_child("ContentChannels")->add_child_text ("2");
	{
		/* L -> L */
		xmlpp::Element* map = mapping->add_child ("Map");
		map->add_child("ContentIndex")->add_child_text ("0");
		map->add_child("DCP")->add_child_text ("0");
	}
	{
		/* L -> C */
		xmlpp::Element* map = mapping->add_child ("Map");
		map->add_child("ContentIndex")->add_child_text ("0");
		map->add_child("DCP")->add_child_text ("2");
	}
	{
		/* R -> R */
		xmlpp::Element* map = mapping->add_child ("Map");
		map->add_child("ContentIndex")->add_child_text ("1");
		map->add_child("DCP")->add_child_text ("1");
	}
	{
		/* R -> C */
		xmlpp::Element* map = mapping->add_child ("Map");
		map->add_child("ContentIndex")->add_child_text ("1");
		map->add_child("DCP")->add_child_text ("2");
	}
		
	FFmpegAudioStream a (shared_ptr<cxml::Node> (new cxml::Node (root)), 5);

	BOOST_CHECK_EQUAL (a.identifier(), "4");
	BOOST_CHECK_EQUAL (a.frame_rate, 44100);
	BOOST_CHECK_EQUAL (a.channels, 2);
	BOOST_CHECK_EQUAL (a.name, "hello there world");
	BOOST_CHECK_EQUAL (a.mapping.content_channels(), 2);

	BOOST_CHECK_EQUAL (a.mapping.get (0, libdcp::LEFT), 1);
	BOOST_CHECK_EQUAL (a.mapping.get (0, libdcp::RIGHT), 0);
	BOOST_CHECK_EQUAL (a.mapping.get (0, libdcp::CENTRE), 1);
	BOOST_CHECK_EQUAL (a.mapping.get (1, libdcp::LEFT), 0);
	BOOST_CHECK_EQUAL (a.mapping.get (1, libdcp::RIGHT), 1);
	BOOST_CHECK_EQUAL (a.mapping.get (1, libdcp::CENTRE), 1);
}

