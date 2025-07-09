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


#include "audio_content.h"
#include "compose.hpp"
#include "config.h"
#include "constants.h"
#include "exceptions.h"
#include "film.h"
#include "frame_rate_change.h"
#include "maths_util.h"
#include "video_content.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::fixed;
using std::list;
using std::make_shared;
using std::pair;
using std::setprecision;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


AudioContent::AudioContent (Content* parent)
	: ContentPart (parent)
	, _delay (Config::instance()->default_audio_delay())
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
			return {};
		}

		/* Otherwise we can drop through to the newer logic */
	}

	if (!node->optional_number_child<double> ("AudioGain")) {
		return {};
	}

	return make_shared<AudioContent>(parent, node);
}


AudioContent::AudioContent (Content* parent, cxml::ConstNodePtr node)
	: ContentPart (parent)
{
	_gain = node->number_child<double> ("AudioGain");
	_delay = node->number_child<int> ("AudioDelay");
	_fade_in = ContentTime(node->optional_number_child<ContentTime::Type>("AudioFadeIn").get_value_or(0));
	_fade_out = ContentTime(node->optional_number_child<ContentTime::Type>("AudioFadeOut").get_value_or(0));
	_use_same_fades_as_video = node->optional_bool_child("AudioUseSameFadesAsVideo").get_value_or(false);
}


AudioContent::AudioContent (Content* parent, vector<shared_ptr<Content>> c)
	: ContentPart (parent)
{
	auto ref = c[0]->audio;
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
AudioContent::as_xml(xmlpp::Element* element) const
{
	boost::mutex::scoped_lock lm (_mutex);
	cxml::add_text_child(element, "AudioGain", fmt::to_string(_gain));
	cxml::add_text_child(element, "AudioDelay", fmt::to_string(_delay));
	cxml::add_text_child(element, "AudioFadeIn", fmt::to_string(_fade_in.get()));
	cxml::add_text_child(element, "AudioFadeOut", fmt::to_string(_fade_out.get()));
	cxml::add_text_child(element, "AudioUseSameFadesAsVideo", _use_same_fades_as_video ? "1" : "0");
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
		s += fmt::format("stream channels {} rate {} ", i->channels(), i->frame_rate());
	}

	return s;
}


void
AudioContent::set_mapping (AudioMapping mapping)
{
	ContentChangeSignaller cc (_parent, AudioContentProperty::STREAMS);

	int c = 0;
	for (auto i: streams()) {
		AudioMapping stream_mapping (i->channels(), MAX_DCP_AUDIO_CHANNELS);
		for (int j = 0; j < i->channels(); ++j) {
			for (int k = 0; k < MAX_DCP_AUDIO_CHANNELS; ++k) {
				stream_mapping.set (j, k, mapping.get(c, k));
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
	for (auto i: streams()) {
		auto mapping = i->mapping ();
		for (int j = 0; j < mapping.input_channels(); ++j) {
			for (int k = 0; k < MAX_DCP_AUDIO_CHANNELS; ++k) {
				if (k < mapping.output_channels()) {
					merged.set (c, k, mapping.get(j, k));
				}
			}
			++c;
		}
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
	if (streams().empty()) {
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
		return fmt::format(_("Some audio will be resampled to {}Hz"), resampled_frame_rate(film));
	}

	if (!not_resampled && resampled) {
		if (same) {
			return fmt::format(_("Audio will be resampled from {}Hz to {}Hz"), common_frame_rate.get(), resampled_frame_rate(film));
		} else {
			return fmt::format(_("Audio will be resampled to {}Hz"), resampled_frame_rate(film));
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
			n.push_back (NamedChannel(fmt::format("{}:{}", stream, j + 1), index++));
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
		stream = streams().front();
	}

	if (stream) {
		p.push_back (UserProperty(UserProperty::AUDIO, _("Channels"), stream->channels()));
		p.push_back (UserProperty(UserProperty::AUDIO, _("Content sample rate"), stream->frame_rate(), _("Hz")));
		if (auto bits = stream->bit_depth()) {
			p.push_back(UserProperty(UserProperty::AUDIO, _("Content bit depth"), *bits, _("bits")));
		}
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

	p.push_back (UserProperty(UserProperty::AUDIO, _("DCP sample rate"), resampled_frame_rate(film), _("Hz")));
	p.push_back (UserProperty(UserProperty::LENGTH, _("Full length in video frames at DCP rate"), c.frames_round (frc.dcp)));

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
	ContentChangeSignaller cc (_parent, AudioContentProperty::STREAMS);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams.push_back (stream);
	}
}


void
AudioContent::set_stream (AudioStreamPtr stream)
{
	ContentChangeSignaller cc (_parent, AudioContentProperty::STREAMS);

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
	set_fade_in (c->fade_in());
	set_fade_out (c->fade_out());

	auto const streams_to_take = std::min(_streams.size(), c->_streams.size());

	for (auto i = 0U; i < streams_to_take; ++i) {
		auto mapping = _streams[i]->mapping();
		mapping.take_from(c->_streams[i]->mapping());
		_streams[i]->set_mapping(mapping);
	}
}


void
AudioContent::modify_position (shared_ptr<const Film> film, DCPTime& pos) const
{
	pos = pos.round (film->audio_frame_rate());
}


void
AudioContent::modify_trim_start(shared_ptr<const Film> film, ContentTime& trim) const
{
	/* When this trim is used it the audio will have been resampled, and using the
	 * DCP rate here reduces the chance of rounding errors causing audio glitches
	 * due to errors in placement of audio frames (#2373).
	 */
	trim = trim.round(film ? film->audio_frame_rate() : 48000);
}


ContentTime
AudioContent::fade_in () const
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_use_same_fades_as_video && _parent->video) {
		return dcpomatic::ContentTime::from_frames(_parent->video->fade_in(), _parent->video_frame_rate().get_value_or(24));
	}

	return _fade_in;
}


ContentTime
AudioContent::fade_out () const
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_use_same_fades_as_video && _parent->video) {
		return dcpomatic::ContentTime::from_frames(_parent->video->fade_out(), _parent->video_frame_rate().get_value_or(24));
	}

	return _fade_out;
}


bool
AudioContent::use_same_fades_as_video() const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _use_same_fades_as_video;
}


void
AudioContent::set_fade_in (ContentTime t)
{
	maybe_set (_fade_in, t, AudioContentProperty::FADE_IN);
}


void
AudioContent::set_fade_out (ContentTime t)
{
	maybe_set (_fade_out, t, AudioContentProperty::FADE_OUT);
}


void
AudioContent::set_use_same_fades_as_video (bool s)
{
	maybe_set (_use_same_fades_as_video, s, AudioContentProperty::USE_SAME_FADES_AS_VIDEO);
}


vector<float>
AudioContent::fade (AudioStreamPtr stream, Frame frame, Frame length, int frame_rate) const
{
	auto const in = fade_in().frames_round(frame_rate);
	auto const out = fade_out().frames_round(frame_rate);

	/* Where the start trim ends, at frame_rate */
	auto const trim_start = _parent->trim_start().frames_round(frame_rate);
	/* Where the end trim starts within the whole length of the content, at frame_rate */
	auto const trim_end = ContentTime(ContentTime::from_frames(stream->length(), stream->frame_rate()) - _parent->trim_end()).frames_round(frame_rate);

	if (
		(in == 0  || (frame >= (trim_start + in))) &&
		(out == 0 || ((frame + length) < (trim_end - out)))
	   ) {
		/* This section starts after the fade in and ends before the fade out */
		return {};
	}

	/* Start position relative to the start of the fade in */
	auto in_start = frame - trim_start;
	/* Start position relative to the start of the fade out */
	auto out_start = frame - (trim_end - out);

	vector<float> coeffs(length);
	for (auto coeff = 0; coeff < length; ++coeff) {
		coeffs[coeff] = 1.0;
		if (in) {
			coeffs[coeff] *= logarithmic_fade_in_curve(static_cast<float>(in_start + coeff) / in);
		}
		if (out) {
			coeffs[coeff] *= logarithmic_fade_out_curve(static_cast<float>(out_start + coeff) / out);
		}
	}

	return coeffs;
}

