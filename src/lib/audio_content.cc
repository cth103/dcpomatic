/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "audio_content.h"
#include "film.h"
#include "exceptions.h"
#include "config.h"
#include "frame_rate_change.h"
#include "compose.hpp"
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::vector;
using std::fixed;
using std::list;
using std::pair;
using std::setprecision;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using boost::optional;
using dcp::raw_convert;
using namespace dcpomatic;

/** Something stream-related has changed */
int const AudioContentProperty::STREAMS = 200;
int const AudioContentProperty::GAIN = 201;
int const AudioContentProperty::DELAY = 202;

AudioContent::AudioContent (Content* parent)
	: ContentPart (parent)
	, _gain (0)
	, _delay (Config::instance()->default_audio_delay ())
{

}

shared_ptr<AudioContent>
AudioContent::from_xml (Content* parent, cxml::ConstNodePtr node, int version)
{
	if (version < 34) {
		/* With old metadata FFmpeg content has the audio-related tags even with no
		   audio streams, so check for that.
		*/
		if (node->string_child("Type") == "FFmpeg" && node->node_children("AudioStream").empty()) {
			return shared_ptr<AudioContent> ();
		}

		/* Otherwise we can drop through to the newer logic */
	}

	if (!node->optional_number_child<double> ("AudioGain")) {
		return shared_ptr<AudioContent> ();
	}

	return shared_ptr<AudioContent> (new AudioContent (parent, node));
}

AudioContent::AudioContent (Content* parent, cxml::ConstNodePtr node)
	: ContentPart (parent)
{
	_gain = node->number_child<double> ("AudioGain");
	_delay = node->number_child<int> ("AudioDelay");

	/* Backwards compatibility */
	optional<double> r = node->optional_number_child<double> ("AudioVideoFrameRate");
	if (r) {
		_parent->set_video_frame_rate (r.get ());
	}
}

AudioContent::AudioContent (Content* parent, vector<shared_ptr<Content> > c)
	: ContentPart (parent)
{
	shared_ptr<AudioContent> ref = c[0]->audio;
	DCPOMATIC_ASSERT (ref);

	for (size_t i = 1; i < c.size(); ++i) {
		if (c[i]->audio->gain() != ref->gain()) {
			throw JoinError (_("Content to be joined must have the same audio gain."));
		}

		if (c[i]->audio->delay() != ref->delay()) {
			throw JoinError (_("Content to be joined must have the same audio delay."));
		}
	}

	_gain = ref->gain ();
	_delay = ref->delay ();
	_streams = ref->streams ();
}

void
AudioContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("AudioGain")->add_child_text (raw_convert<string> (_gain));
	node->add_child("AudioDelay")->add_child_text (raw_convert<string> (_delay));
}

void
AudioContent::set_gain (double g)
{
	maybe_set (_gain, g, AudioContentProperty::GAIN);
}

void
AudioContent::set_delay (int d)
{
	maybe_set (_delay, d, AudioContentProperty::DELAY);
}

string
AudioContent::technical_summary () const
{
	string s = "audio: ";
	for (auto i: streams()) {
		s += String::compose ("stream channels %1 rate %2 ", i->channels(), i->frame_rate());
	}

	return s;
}

void
AudioContent::set_mapping (AudioMapping mapping)
{
	ChangeSignaller<Content> cc (_parent, AudioContentProperty::STREAMS);

	int c = 0;
	for (auto i: streams()) {
		AudioMapping stream_mapping (i->channels (), MAX_DCP_AUDIO_CHANNELS);
		for (int j = 0; j < i->channels(); ++j) {
			for (int k = 0; k < MAX_DCP_AUDIO_CHANNELS; ++k) {
				stream_mapping.set (j, k, mapping.get (c, k));
			}
			++c;
		}
		i->set_mapping (stream_mapping);
	}
}

AudioMapping
AudioContent::mapping () const
{
	int channels = 0;
	for (auto i: streams()) {
		channels += i->channels ();
	}

	AudioMapping merged (channels, MAX_DCP_AUDIO_CHANNELS);
	merged.make_zero ();

	int c = 0;
	int s = 0;
	for (auto i: streams()) {
		AudioMapping mapping = i->mapping ();
		for (int j = 0; j < mapping.input_channels(); ++j) {
			for (int k = 0; k < MAX_DCP_AUDIO_CHANNELS; ++k) {
				if (k < mapping.output_channels()) {
					merged.set (c, k, mapping.get (j, k));
				}
			}
			++c;
		}
		++s;
	}

	return merged;
}

/** @return the frame rate that this content should be resampled to in order
 *  that it is in sync with the active video content at its start time.
 */
int
AudioContent::resampled_frame_rate (shared_ptr<const Film> film) const
{
	double t = film->audio_frame_rate ();

	FrameRateChange frc (film, _parent);

	/* Compensate if the DCP is being run at a different frame rate
	   to the source; that is, if the video is run such that it will
	   look different in the DCP compared to the source (slower or faster).
	*/

	if (frc.change_speed) {
		t /= frc.speed_up;
	}

	return lrint (t);
}

string
AudioContent::processing_description (shared_ptr<const Film> film) const
{
	if (streams().empty ()) {
		return "";
	}

	/* Possible answers are:
	   1. all audio will be resampled from x to y.
	   2. all audio will be resampled to y (from a variety of rates)
	   3. some audio will be resampled to y (from a variety of rates)
	   4. nothing will be resampled.
	*/

	bool not_resampled = false;
	bool resampled = false;
	bool same = true;

	optional<int> common_frame_rate;
	for (auto i: streams()) {
		if (i->frame_rate() != resampled_frame_rate(film)) {
			resampled = true;
		} else {
			not_resampled = true;
		}

		if (common_frame_rate && common_frame_rate != i->frame_rate ()) {
			same = false;
		}
		common_frame_rate = i->frame_rate ();
	}

	if (not_resampled && !resampled) {
		return _("Audio will not be resampled");
	}

	if (not_resampled && resampled) {
		return String::compose (_("Some audio will be resampled to %1Hz"), resampled_frame_rate(film));
	}

	if (!not_resampled && resampled) {
		if (same) {
			return String::compose (_("Audio will be resampled from %1Hz to %2Hz"), common_frame_rate.get(), resampled_frame_rate(film));
		} else {
			return String::compose (_("Audio will be resampled to %1Hz"), resampled_frame_rate(film));
		}
	}

	return "";
}

/** @return User-visible names of each of our audio channels */
vector<NamedChannel>
AudioContent::channel_names () const
{
	vector<NamedChannel> n;

	int index = 0;
	int stream = 1;
	for (auto i: streams()) {
		for (int j = 0; j < i->channels(); ++j) {
			n.push_back (NamedChannel(String::compose ("%1:%2", stream, j + 1), index++));
		}
		++stream;
	}

	return n;
}

void
AudioContent::add_properties (shared_ptr<const Film> film, list<UserProperty>& p) const
{
	shared_ptr<const AudioStream> stream;
	if (streams().size() == 1) {
		stream = streams().front ();
	}

	if (stream) {
		p.push_back (UserProperty (UserProperty::AUDIO, _("Channels"), stream->channels ()));
		p.push_back (UserProperty (UserProperty::AUDIO, _("Content audio sample rate"), stream->frame_rate(), _("Hz")));
	}

	FrameRateChange const frc (_parent->active_video_frame_rate(film), film->video_frame_rate());
	ContentTime const c (_parent->full_length(film), frc);

	p.push_back (
		UserProperty (UserProperty::LENGTH, _("Full length in video frames at content rate"), c.frames_round(frc.source))
		);

	if (stream) {
		p.push_back (
			UserProperty (
				UserProperty::LENGTH,
				_("Full length in audio samples at content rate"),
				c.frames_round (stream->frame_rate ())
				)
			);
	}

	p.push_back (UserProperty (UserProperty::AUDIO, _("DCP sample rate"), resampled_frame_rate(film), _("Hz")));
	p.push_back (UserProperty (UserProperty::LENGTH, _("Full length in video frames at DCP rate"), c.frames_round (frc.dcp)));

	if (stream) {
		p.push_back (
			UserProperty (
				UserProperty::LENGTH,
				_("Full length in audio samples at DCP rate"),
				c.frames_round(resampled_frame_rate(film))
				)
			);
	}
}

void
AudioContent::set_streams (vector<AudioStreamPtr> streams)
{
	ChangeSignaller<Content> cc (_parent, AudioContentProperty::STREAMS);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams = streams;
	}
}

AudioStreamPtr
AudioContent::stream () const
{
	boost::mutex::scoped_lock lm (_mutex);
	DCPOMATIC_ASSERT (_streams.size() == 1);
	return _streams.front ();
}

void
AudioContent::add_stream (AudioStreamPtr stream)
{
	ChangeSignaller<Content> cc (_parent, AudioContentProperty::STREAMS);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams.push_back (stream);
	}
}

void
AudioContent::set_stream (AudioStreamPtr stream)
{
	ChangeSignaller<Content> cc (_parent, AudioContentProperty::STREAMS);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams.clear ();
		_streams.push_back (stream);
	}
}

void
AudioContent::take_settings_from (shared_ptr<const AudioContent> c)
{
	set_gain (c->_gain);
	set_delay (c->_delay);

	size_t i = 0;
	size_t j = 0;

	while (i < _streams.size() && j < c->_streams.size()) {
		_streams[i]->set_mapping (c->_streams[j]->mapping());
		++i;
		++j;
	}
}

void
AudioContent::modify_position (shared_ptr<const Film> film, DCPTime& pos) const
{
	pos = pos.round (film->audio_frame_rate());
}

void
AudioContent::modify_trim_start (ContentTime& trim) const
{
	DCPOMATIC_ASSERT (!_streams.empty());
	/* XXX: we're in trouble if streams have different rates */
	trim = trim.round (_streams.front()->frame_rate());
}
