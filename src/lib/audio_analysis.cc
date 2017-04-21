/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "audio_analysis.h"
#include "cross.h"
#include "util.h"
#include "playlist.h"
#include "audio_content.h"
#include <dcp/raw_convert.h>
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
using std::pair;
using std::make_pair;
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

int const AudioAnalysis::_current_state_version = 2;

AudioAnalysis::AudioAnalysis (int channels)
{
	_data.resize (channels);
}

AudioAnalysis::AudioAnalysis (boost::filesystem::path filename)
{
	cxml::Document f ("AudioAnalysis");
	f.read_file (filename);

	if (f.optional_number_child<int>("Version").get_value_or(1) < _current_state_version) {
		/* Too old.  Throw an exception so that this analysis is re-run. */
		throw OldFormatError ("Audio analysis file is too old");
	}

	BOOST_FOREACH (cxml::NodePtr i, f.node_children ("Channel")) {
		vector<AudioPoint> channel;

		BOOST_FOREACH (cxml::NodePtr j, i->node_children ("Point")) {
			channel.push_back (AudioPoint (j));
		}

		_data.push_back (channel);
	}

	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children ("SamplePeak")) {
		_sample_peak.push_back (
			PeakTime (
				dcp::raw_convert<float>(i->content()), DCPTime(i->number_attribute<Frame>("Time"))
				)
			);
	}

	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children ("TruePeak")) {
		_true_peak.push_back (dcp::raw_convert<float> (i->content ()));
	}

	_integrated_loudness = f.optional_number_child<float> ("IntegratedLoudness");
	_loudness_range = f.optional_number_child<float> ("LoudnessRange");

	_analysis_gain = f.optional_number_child<double> ("AnalysisGain");
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

	root->add_child("Version")->add_child_text (raw_convert<string> (_current_state_version));

	BOOST_FOREACH (vector<AudioPoint>& i, _data) {
		xmlpp::Element* channel = root->add_child ("Channel");
		BOOST_FOREACH (AudioPoint& j, i) {
			j.as_xml (channel->add_child ("Point"));
		}
	}

	for (size_t i = 0; i < _sample_peak.size(); ++i) {
		xmlpp::Element* n = root->add_child("SamplePeak");
		n->add_child_text (raw_convert<string> (_sample_peak[i].peak));
		n->set_attribute ("Time", raw_convert<string> (_sample_peak[i].time.get()));
	}

	BOOST_FOREACH (float i, _true_peak) {
		root->add_child("TruePeak")->add_child_text (raw_convert<string> (i));
	}

	if (_integrated_loudness) {
		root->add_child("IntegratedLoudness")->add_child_text (raw_convert<string> (_integrated_loudness.get ()));
	}

	if (_loudness_range) {
		root->add_child("LoudnessRange")->add_child_text (raw_convert<string> (_loudness_range.get ()));
	}

	if (_analysis_gain) {
		root->add_child("AnalysisGain")->add_child_text (raw_convert<string> (_analysis_gain.get ()));
	}

	doc->write_to_file_formatted (filename.string ());
}

float
AudioAnalysis::gain_correction (shared_ptr<const Playlist> playlist)
{
	if (playlist->content().size() == 1 && analysis_gain ()) {
		/* In this case we know that the analysis was of a single piece of content and
		   we know that content's gain when the analysis was run.  Hence we can work out
		   what correction is now needed to make it look `right'.
		*/
		DCPOMATIC_ASSERT (playlist->content().front()->audio);
		return playlist->content().front()->audio->gain() - analysis_gain().get ();
	}

	return 0.0f;
}

/** @return Peak across all channels, and the channel number it is on */
pair<AudioAnalysis::PeakTime, int>
AudioAnalysis::overall_sample_peak () const
{
	DCPOMATIC_ASSERT (!_sample_peak.empty ());

	optional<PeakTime> pt;
	int c;

	for (size_t i = 0; i < _sample_peak.size(); ++i) {
		if (!pt || _sample_peak[i].peak > pt->peak) {
			pt = _sample_peak[i];
			c = i;
		}
	}

	return make_pair (pt.get(), c);
}

optional<float>
AudioAnalysis::overall_true_peak () const
{
	optional<float> p;

	BOOST_FOREACH (float i, _true_peak) {
		if (!p || i > *p) {
			p = i;
		}
	}

	return p;
}
