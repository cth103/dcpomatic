/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "audio_mapping.h"
#include "util.h"
#include "md5_digester.h"
#include "raw_convert.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>

using std::list;
using std::cout;
using std::make_pair;
using std::pair;
using std::string;
using std::min;
using std::vector;
using std::abs;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

AudioMapping::AudioMapping ()
	: _input_channels (0)
	, _output_channels (0)
{

}

/** Create an empty AudioMapping.
 *  @param input_channels Number of input channels.
 *  @param output_channels Number of output channels.
 */
AudioMapping::AudioMapping (int input_channels, int output_channels)
{
	setup (input_channels, output_channels);
}

void
AudioMapping::setup (int input_channels, int output_channels)
{
	_input_channels = input_channels;
	_output_channels = output_channels;
	
	_gain.resize (_input_channels);
	for (int i = 0; i < _input_channels; ++i) {
		_gain[i].resize (_output_channels);
	}

	make_zero ();
}

void
AudioMapping::make_zero ()
{
	for (int i = 0; i < _input_channels; ++i) {
		for (int j = 0; j < _output_channels; ++j) {
			_gain[i][j] = 0;
		}
	}
}

AudioMapping::AudioMapping (cxml::ConstNodePtr node, int state_version)
{
	if (state_version < 32) {
		setup (node->number_child<int> ("ContentChannels"), MAX_DCP_AUDIO_CHANNELS);
	} else {
		setup (node->number_child<int> ("InputChannels"), node->number_child<int> ("OutputChannels"));
	}

	if (state_version <= 5) {
		/* Old-style: on/off mapping */
		list<cxml::NodePtr> const c = node->node_children ("Map");
		for (list<cxml::NodePtr>::const_iterator i = c.begin(); i != c.end(); ++i) {
			set ((*i)->number_child<int> ("ContentIndex"), static_cast<dcp::Channel> ((*i)->number_child<int> ("DCP")), 1);
		}
	} else {
		list<cxml::NodePtr> const c = node->node_children ("Gain");
		for (list<cxml::NodePtr>::const_iterator i = c.begin(); i != c.end(); ++i) {
			if (state_version < 32) {
				set (
					(*i)->number_attribute<int> ("Content"),
					static_cast<dcp::Channel> ((*i)->number_attribute<int> ("DCP")),
					raw_convert<float> ((*i)->content ())
					);
			} else {
				set (
					(*i)->number_attribute<int> ("Input"),
					(*i)->number_attribute<int> ("Output"),
					raw_convert<float> ((*i)->content ())
					);
			}
		}
	}
}

void
AudioMapping::set (int input_channel, int output_channel, float g)
{
	_gain[input_channel][output_channel] = g;
}

float
AudioMapping::get (int input_channel, int output_channel) const
{
	return _gain[input_channel][output_channel];
}

void
AudioMapping::as_xml (xmlpp::Node* node) const
{
	node->add_child ("InputChannels")->add_child_text (raw_convert<string> (_input_channels));
	node->add_child ("OutputChannels")->add_child_text (raw_convert<string> (_output_channels));

	for (int c = 0; c < _input_channels; ++c) {
		for (int d = 0; d < _output_channels; ++d) {
			xmlpp::Element* t = node->add_child ("Gain");
			t->set_attribute ("Input", raw_convert<string> (c));
			t->set_attribute ("Output", raw_convert<string> (d));
			t->add_child_text (raw_convert<string> (get (c, d)));
		}
	}
}

/** @return a string which is unique for a given AudioMapping configuration, for
 *  differentiation between different AudioMappings.
 */
string
AudioMapping::digest () const
{
	MD5Digester digester;
	digester.add (_input_channels);
	digester.add (_output_channels);
	for (int i = 0; i < _input_channels; ++i) {
		for (int j = 0; j < _output_channels; ++j) {
			digester.add (_gain[i][j]);
		}
	}

	return digester.get ();
}

list<int>
AudioMapping::mapped_output_channels () const
{
	static float const minus_96_db = 0.000015849;

	list<int> mapped;
	
	for (vector<vector<float> >::const_iterator i = _gain.begin(); i != _gain.end(); ++i) {
		for (size_t j = 0; j < i->size(); ++j) {
			if (abs ((*i)[j]) > minus_96_db) {
				mapped.push_back (j);
			}
		}
	}

	mapped.sort ();
	mapped.unique ();
	
	return mapped;
}

void
AudioMapping::unmap_all ()
{
	for (vector<vector<float> >::iterator i = _gain.begin(); i != _gain.end(); ++i) {
		for (vector<float>::iterator j = i->begin(); j != i->end(); ++j) {
			*j = 0;
		}
	}
}
