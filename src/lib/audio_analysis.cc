/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
#include "audio_content.h"
#include "cross.h"
#include "playlist.h"
#include "util.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>
#include <stdint.h>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <inttypes.h>


using std::cout;
using std::dynamic_pointer_cast;
using std::istream;
using std::list;
using std::make_pair;
using std::make_shared;
using std::max;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using dcp::raw_convert;
using namespace dcpomatic;


int const AudioAnalysis::_current_state_version = 3;


AudioAnalysis::AudioAnalysis (int channels)
{
	_data.resize (channels);
}


AudioAnalysis::AudioAnalysis (boost::filesystem::path filename)
{
	cxml::Document f ("AudioAnalysis");
	f.read_file(dcp::filesystem::fix_long_path(filename));

	if (f.optional_number_child<int>("Version").get_value_or(1) < _current_state_version) {
		/* Too old.  Throw an exception so that this analysis is re-run. */
		throw OldFormatError ("Audio analysis file is too old");
	}

	for (auto i: f.node_children("Channel")) {
		vector<AudioPoint> channel;

		for (auto j: i->node_children("Point")) {
			channel.push_back (AudioPoint(j));
		}

		_data.push_back (channel);
	}

	for (auto i: f.node_children ("SamplePeak")) {
		auto const time = number_attribute<Frame>(i, "Time", "time");
		_sample_peak.push_back(PeakTime(dcp::raw_convert<float>(i->content()), DCPTime(time)));
	}

	for (auto i: f.node_children("TruePeak")) {
		_true_peak.push_back (dcp::raw_convert<float>(i->content()));
	}

	_integrated_loudness = f.optional_number_child<float>("IntegratedLoudness");
	_loudness_range = f.optional_number_child<float>("LoudnessRange");

	_analysis_gain = f.optional_number_child<double>("AnalysisGain");
	_samples_per_point = f.number_child<int64_t>("SamplesPerPoint");
	_sample_rate = f.number_child<int64_t>("SampleRate");

	_leqm = f.optional_number_child<double>("Leqm");
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
	DCPOMATIC_ASSERT (p < points(c));
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
	DCPOMATIC_ASSERT (c < channels());
	return _data[c].size ();
}


void
AudioAnalysis::write (boost::filesystem::path filename)
{
	auto doc = make_shared<xmlpp::Document>();
	auto root = doc->create_root_node("AudioAnalysis");

	cxml::add_text_child(root, "Version", raw_convert<string>(_current_state_version));

	for (auto& i: _data) {
		auto channel = cxml::add_child(root, "Channel");
		for (auto& j: i) {
			j.as_xml(cxml::add_child(channel, "Point"));
		}
	}

	for (size_t i = 0; i < _sample_peak.size(); ++i) {
		auto n = cxml::add_child(root, "SamplePeak");
		n->add_child_text (raw_convert<string> (_sample_peak[i].peak));
		n->set_attribute("time", raw_convert<string> (_sample_peak[i].time.get()));
	}

	for (auto i: _true_peak) {
		cxml::add_text_child(root, "TruePeak", raw_convert<string>(i));
	}

	if (_integrated_loudness) {
		cxml::add_text_child(root, "IntegratedLoudness", raw_convert<string>(_integrated_loudness.get()));
	}

	if (_loudness_range) {
		cxml::add_text_child(root, "LoudnessRange", raw_convert<string>(_loudness_range.get()));
	}

	if (_analysis_gain) {
		cxml::add_text_child(root, "AnalysisGain", raw_convert<string>(_analysis_gain.get()));
	}

	cxml::add_text_child(root, "SamplesPerPoint", raw_convert<string>(_samples_per_point));
	cxml::add_text_child(root, "SampleRate", raw_convert<string>(_sample_rate));

	if (_leqm) {
		cxml::add_text_child(root, "Leqm", raw_convert<string>(*_leqm));
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
		return playlist->content().front()->audio->gain() - analysis_gain().get();
	}

	return 0.0f;
}


/** @return Peak across all channels, and the channel number it is on */
pair<AudioAnalysis::PeakTime, int>
AudioAnalysis::overall_sample_peak () const
{
	DCPOMATIC_ASSERT (!_sample_peak.empty());

	optional<PeakTime> pt;
	int c = 0;

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

	for (auto i: _true_peak) {
		if (!p || i > *p) {
			p = i;
		}
	}

	return p;
}

