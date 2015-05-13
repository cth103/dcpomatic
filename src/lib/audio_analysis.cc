/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "audio_analysis.h"
#include "cross.h"
#include "util.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <stdint.h>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <inttypes.h>

using std::ostream;
using std::istream;
using std::string;
using std::vector;
using std::cout;
using std::max;
using std::list;
using boost::shared_ptr;

AudioPoint::AudioPoint ()
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = 0;
	}
}

AudioPoint::AudioPoint (cxml::ConstNodePtr node)
{
	_data[PEAK] = node->number_child<float> ("Peak");
	_data[RMS] = node->number_child<float> ("RMS");
}

AudioPoint::AudioPoint (AudioPoint const & other)
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}
}

AudioPoint &
AudioPoint::operator= (AudioPoint const & other)
{
	if (this == &other) {
		return *this;
	}
	
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}

	return *this;
}

void
AudioPoint::as_xml (xmlpp::Element* parent) const
{
	parent->add_child ("Peak")->add_child_text (raw_convert<string> (_data[PEAK]));
	parent->add_child ("RMS")->add_child_text (raw_convert<string> (_data[RMS]));
}
	
AudioAnalysis::AudioAnalysis (int channels)
{
	_data.resize (channels);
}

AudioAnalysis::AudioAnalysis (boost::filesystem::path filename)
{
	cxml::Document f ("AudioAnalysis");
	f.read_file (filename);

	BOOST_FOREACH (cxml::NodePtr i, f.node_children ("Channel")) {
		vector<AudioPoint> channel;

		BOOST_FOREACH (cxml::NodePtr j, i->node_children ("Point")) {
			channel.push_back (AudioPoint (j));
		}

		_data.push_back (channel);
	}

	_peak = f.number_child<float> ("Peak");
	_peak_time = DCPTime (f.number_child<DCPTime::Type> ("PeakTime"));
}

void
AudioAnalysis::add_point (int c, AudioPoint const & p)
{
	DCPOMATIC_ASSERT (c < channels ());
	_data[c].push_back (p);
}

AudioPoint
AudioAnalysis::get_point (int c, int p) const
{
	DCPOMATIC_ASSERT (p < points (c));
	return _data[c][p];
}

int
AudioAnalysis::channels () const
{
	return _data.size ();
}

int
AudioAnalysis::points (int c) const
{
	DCPOMATIC_ASSERT (c < channels ());
	return _data[c].size ();
}

void
AudioAnalysis::write (boost::filesystem::path filename)
{
	shared_ptr<xmlpp::Document> doc (new xmlpp::Document);
	xmlpp::Element* root = doc->create_root_node ("AudioAnalysis");

	BOOST_FOREACH (vector<AudioPoint>& i, _data) {
		xmlpp::Element* channel = root->add_child ("Channel");
		BOOST_FOREACH (AudioPoint& j, i) {
			j.as_xml (channel->add_child ("Point"));
		}
	}

	if (_peak) {
		root->add_child("Peak")->add_child_text (raw_convert<string> (_peak.get ()));
		root->add_child("PeakTime")->add_child_text (raw_convert<string> (_peak_time.get().get ()));
	}

	doc->write_to_file_formatted (filename.string ());
}
