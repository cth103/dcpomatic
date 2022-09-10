/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_mapping.h"
#include "audio_processor.h"
#include "constants.h"
#include "dcpomatic_assert.h"
#include "digester.h"
#include "util.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/regex.hpp>
#include <iostream>


using std::list;
using std::cout;
using std::make_pair;
using std::pair;
using std::string;
using std::min;
using std::vector;
using std::abs;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using boost::optional;
using dcp::raw_convert;


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
	_gain.resize(input_channels);
	for (int i = 0; i < input_channels; ++i) {
		_gain[i].resize(output_channels);
	}

	make_zero ();
}


void
AudioMapping::make_zero ()
{
	for (auto& input: _gain) {
		for (auto& output: input) {
			output = 0;
		}
	}
}


struct ChannelRegex
{
	ChannelRegex (string regex_, int channel_)
		: regex (regex_)
		, channel (channel_)
	{}

	string regex;
	int channel;
};


void
AudioMapping::make_default (AudioProcessor const * processor, optional<boost::filesystem::path> filename)
{
	static ChannelRegex const regex[] = {
		ChannelRegex(".*[\\._-]L[\\._-].*", 0),
		ChannelRegex(".*[\\._-]R[\\._-].*", 1),
		ChannelRegex(".*[\\._-]C[\\._-].*", 2),
		ChannelRegex(".*[\\._-]Lfe[\\._-].*", 3),
		ChannelRegex(".*[\\._-]LFE[\\._-].*", 3),
		ChannelRegex(".*[\\._-]Lss[\\._-].*", 4),
		ChannelRegex(".*[\\._-]Lsr[\\._-].*", 6),
		ChannelRegex(".*[\\._-]Lrs[\\._-].*", 6),
		ChannelRegex(".*[\\._-]Ls[\\._-].*", 4),
		ChannelRegex(".*[\\._-]Rss[\\._-].*", 5),
		ChannelRegex(".*[\\._-]Rsr[\\._-].*", 7),
		ChannelRegex(".*[\\._-]Rrs[\\._-].*", 7),
		ChannelRegex(".*[\\._-]Rs[\\._-].*", 5),
	};

	static int const regexes = sizeof(regex) / sizeof(*regex);

	if (processor) {
		processor->make_audio_mapping_default (*this);
	} else {
		make_zero ();
		if (input_channels() == 1) {
			bool guessed = false;

			/* See if we can guess where this stream should go */
			if (filename) {
				for (int i = 0; i < regexes; ++i) {
					boost::regex e (regex[i].regex, boost::regex::icase);
					if (boost::regex_match(filename->filename().string(), e) && regex[i].channel < output_channels()) {
						set (0, regex[i].channel, 1);
						guessed = true;
					}
				}
			}

			if (!guessed) {
				/* If we have no idea, just put it on centre */
				set (0, static_cast<int>(dcp::Channel::CENTRE), 1);
			}
		} else {
			/* 1:1 mapping */
			for (int i = 0; i < min (input_channels(), output_channels()); ++i) {
				set (i, i, 1);
			}
		}
	}
}


AudioMapping::AudioMapping (cxml::ConstNodePtr node, int state_version)
{
	if (state_version < 32) {
		setup (node->number_child<int>("ContentChannels"), MAX_DCP_AUDIO_CHANNELS);
	} else {
		setup (node->number_child<int>("InputChannels"), node->number_child<int>("OutputChannels"));
	}

	if (state_version <= 5) {
		/* Old-style: on/off mapping */
		for (auto i: node->node_children ("Map")) {
			set (i->number_child<int>("ContentIndex"), i->number_child<int>("DCP"), 1);
		}
	} else {
		for (auto i: node->node_children("Gain")) {
			if (state_version < 32) {
				set (
					i->number_attribute<int>("Content"),
					i->number_attribute<int>("DCP"),
					raw_convert<float>(i->content())
					);
			} else {
				set (
					number_attribute<int>(i, "Input", "input"),
					number_attribute<int>(i, "Output", "output"),
					raw_convert<float>(i->content())
					);
			}
		}
	}
}


void
AudioMapping::set (dcp::Channel input_channel, int output_channel, float g)
{
	set (static_cast<int>(input_channel), output_channel, g);
}


void
AudioMapping::set (int input_channel, dcp::Channel output_channel, float g)
{
	set (input_channel, static_cast<int>(output_channel), g);
}


void
AudioMapping::set (int input_channel, int output_channel, float g)
{
	DCPOMATIC_ASSERT (input_channel < int(_gain.size()));
	DCPOMATIC_ASSERT (output_channel < int(_gain[0].size()));
	_gain[input_channel][output_channel] = g;
}


float
AudioMapping::get (int input_channel, dcp::Channel output_channel) const
{
	return get (input_channel, static_cast<int>(output_channel));
}


float
AudioMapping::get (int input_channel, int output_channel) const
{
	DCPOMATIC_ASSERT (input_channel < int (_gain.size()));
	DCPOMATIC_ASSERT (output_channel < int (_gain[0].size()));
	return _gain[input_channel][output_channel];
}


void
AudioMapping::as_xml (xmlpp::Node* node) const
{
	auto const input = input_channels();
	auto const output = output_channels();

	node->add_child("InputChannels")->add_child_text(raw_convert<string>(input));
	node->add_child("OutputChannels")->add_child_text(raw_convert<string>(output));

	for (int c = 0; c < input; ++c) {
		for (int d = 0; d < output; ++d) {
			auto t = node->add_child ("Gain");
			t->set_attribute("input", raw_convert<string>(c));
			t->set_attribute("output", raw_convert<string>(d));
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
	Digester digester;
	digester.add(input_channels());
	digester.add(output_channels());
	for (auto const& input: _gain) {
		for (auto output: input) {
			digester.add(output);
		}
	}

	return digester.get ();
}


list<int>
AudioMapping::mapped_output_channels () const
{
	static float const minus_96_db = 0.000015849;

	list<int> mapped;

	for (auto const& i: _gain) {
		for (auto j: dcp::used_audio_channels()) {
			if (abs(i[static_cast<int>(j)]) > minus_96_db) {
				mapped.push_back (static_cast<int>(j));
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
	for (auto& i: _gain) {
		for (auto& j: i) {
			j = 0;
		}
	}
}


void
AudioMapping::take_from(AudioMapping const& other)
{
	auto input = std::min(input_channels(), other.input_channels());
	auto output = std::min(output_channels(), other.output_channels());

	for (auto i = 0; i < input; ++i) {
		for (auto o = 0; o < output; ++o) {
			set(i, o, other.get(i, o));
		}
	}
}

