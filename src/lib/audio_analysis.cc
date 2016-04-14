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
#include "playlist.h"
#include "audio_content.h"
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
using boost::dynamic_pointer_cast;

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

	_sample_peak = f.optional_number_child<float> ("Peak");
	if (!_sample_peak) {
		/* New key */
		_sample_peak = f.optional_number_child<float> ("SamplePeak");
	}

	if (f.optional_number_child<DCPTime::Type> ("PeakTime")) {
		_sample_peak_time = DCPTime (f.number_child<DCPTime::Type> ("PeakTime"));
	} else if (f.optional_number_child<DCPTime::Type> ("SamplePeakTime")) {
		_sample_peak_time = DCPTime (f.number_child<DCPTime::Type> ("SamplePeakTime"));
	}

	_true_peak = f.optional_number_child<float> ("TruePeak");
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

	BOOST_FOREACH (vector<AudioPoint>& i, _data) {
		xmlpp::Element* channel = root->add_child ("Channel");
		BOOST_FOREACH (AudioPoint& j, i) {
			j.as_xml (channel->add_child ("Point"));
		}
	}

	if (_sample_peak) {
		root->add_child("SamplePeak")->add_child_text (raw_convert<string> (_sample_peak.get ()));
		root->add_child("SamplePeakTime")->add_child_text (raw_convert<string> (_sample_peak_time.get().get ()));
	}

	if (_true_peak) {
		root->add_child("TruePeak")->add_child_text (raw_convert<string> (_true_peak.get ()));
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
		return playlist->content().front()->audio->audio_gain() - analysis_gain().get ();
	}

	return 0.0f;
}
