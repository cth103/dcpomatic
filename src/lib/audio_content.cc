/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "audio_content.h"
#include "film.h"
#include "exceptions.h"
#include "config.h"
#include "frame_rate_change.h"
#include "raw_convert.h"
#include "compose.hpp"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::vector;
using std::stringstream;
using std::fixed;
using std::list;
using std::pair;
using std::setprecision;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

/** Something stream-related has changed */
int const AudioContentProperty::AUDIO_STREAMS = 200;
int const AudioContentProperty::AUDIO_GAIN = 201;
int const AudioContentProperty::AUDIO_DELAY = 202;
int const AudioContentProperty::AUDIO_VIDEO_FRAME_RATE = 203;

AudioContent::AudioContent (Content* parent, shared_ptr<const Film> film)
	: ContentPart (parent, film)
	, _audio_gain (0)
	, _audio_delay (Config::instance()->default_audio_delay ())
{

}

AudioContent::AudioContent (Content* parent, shared_ptr<const Film> film, cxml::ConstNodePtr node)
	: ContentPart (parent, film)
{
	_audio_gain = node->number_child<double> ("AudioGain");
	_audio_delay = node->number_child<int> ("AudioDelay");
	_audio_video_frame_rate = node->optional_number_child<double> ("AudioVideoFrameRate");
}

AudioContent::AudioContent (Content* parent, shared_ptr<const Film> film, vector<shared_ptr<Content> > c)
	: ContentPart (parent, film)
{
	shared_ptr<AudioContent> ref = c[0]->audio;
	DCPOMATIC_ASSERT (ref);

	for (size_t i = 1; i < c.size(); ++i) {
		if (c[i]->audio->audio_gain() != ref->audio_gain()) {
			throw JoinError (_("Content to be joined must have the same audio gain."));
		}

		if (c[i]->audio->audio_delay() != ref->audio_delay()) {
			throw JoinError (_("Content to be joined must have the same audio delay."));
		}

		if (ac->audio_video_frame_rate() != ref->audio_video_frame_rate()) {
			throw JoinError (_("Content to be joined must have the same video frame rate."));
		}
	}

	_audio_gain = ref->audio_gain ();
	_audio_delay = ref->audio_delay ();
	/* Preserve the optional<> part of this */
	_audio_video_frame_rate = ref->_audio_video_frame_rate;
	_streams = ref->streams ();
}

void
AudioContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("AudioGain")->add_child_text (raw_convert<string> (_audio_gain));
	node->add_child("AudioDelay")->add_child_text (raw_convert<string> (_audio_delay));
	if (_audio_video_frame_rate) {
		node->add_child("AudioVideoFrameRate")->add_child_text (raw_convert<string> (_audio_video_frame_rate.get()));
	}
}

void
AudioContent::set_audio_gain (double g)
{
	maybe_set (_audio_gain, g, AudioContentProperty::AUDIO_GAIN);
}

void
AudioContent::set_audio_delay (int d)
{
	maybe_set (_audio_delay, d, AudioContentProperty::AUDIO_DELAY);
}

string
AudioContent::technical_summary () const
{
	string s = "audio :";
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
		s += String::compose ("stream channels %1 rate %2", i->channels(), i->frame_rate());
	}

	return s;
}

void
AudioContent::set_audio_mapping (AudioMapping mapping)
{
	int c = 0;
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
		AudioMapping stream_mapping (i->channels (), MAX_DCP_AUDIO_CHANNELS);
		for (int j = 0; j < i->channels(); ++j) {
			for (int k = 0; k < MAX_DCP_AUDIO_CHANNELS; ++k) {
				stream_mapping.set (j, k, mapping.get (c, k));
			}
			++c;
		}
		i->set_mapping (stream_mapping);
	}

	_parent->signal_changed (AudioContentProperty::AUDIO_STREAMS);
}

AudioMapping
AudioContent::audio_mapping () const
{
	int channels = 0;
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
		channels += i->channels ();
	}

	AudioMapping merged (channels, MAX_DCP_AUDIO_CHANNELS);
	merged.make_zero ();

	int c = 0;
	int s = 0;
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
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
AudioContent::resampled_audio_frame_rate () const
{
	/* Resample to a DCI-approved sample rate */
	double t = has_rate_above_48k() ? 96000 : 48000;

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	FrameRateChange frc (audio_video_frame_rate(), film->video_frame_rate());

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
AudioContent::processing_description () const
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
	BOOST_FOREACH (AudioStreamPtr i, streams()) {
		if (i->frame_rate() != resampled_audio_frame_rate()) {
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
		return String::compose (_("Some audio will be resampled to %1kHz"), resampled_audio_frame_rate ());
	}

	if (!not_resampled && resampled) {
		if (same) {
			return String::compose (_("Audio will be resampled from %1kHz to %2kHz"), common_frame_rate.get(), resampled_audio_frame_rate ());
		} else {
			return String::compose (_("Audio will be resampled to %1kHz"), resampled_audio_frame_rate ());
		}
	}

	return "";
}

/** @return true if any stream in this content has a sampling rate of more than 48kHz */
bool
AudioContent::has_rate_above_48k () const
{
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
		if (i->frame_rate() > 48000) {
			return true;
		}
	}

	return false;
}

/** @return User-visible names of each of our audio channels */
vector<string>
AudioContent::audio_channel_names () const
{
	vector<string> n;

	int t = 1;
	BOOST_FOREACH (AudioStreamPtr i, streams ()) {
		for (int j = 0; j < i->channels(); ++j) {
			n.push_back (String::compose ("%1:%2", t, j + 1));
		}
		++t;
	}

	return n;
}

void
AudioContent::add_properties (list<UserProperty>& p) const
{
	shared_ptr<const AudioStream> stream;
	if (streams().size() == 1) {
		stream = streams().front ();
	}

	if (stream) {
		p.push_back (UserProperty (_("Audio"), _("Channels"), stream->channels ()));
		p.push_back (UserProperty (_("Audio"), _("Content audio frame rate"), stream->frame_rate(), _("Hz")));
	}

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	FrameRateChange const frc (audio_video_frame_rate(), film->video_frame_rate());
	ContentTime const c (_parent->full_length(), frc);

	p.push_back (
		UserProperty (_("Length"), _("Full length in video frames at content rate"), c.frames_round(frc.source))
		);

	if (stream) {
		p.push_back (
			UserProperty (
				_("Length"),
				_("Full length in audio frames at content rate"),
				c.frames_round (stream->frame_rate ())
				)
			);
	}

	p.push_back (UserProperty (_("Audio"), _("DCP frame rate"), resampled_audio_frame_rate (), _("Hz")));
	p.push_back (UserProperty (_("Length"), _("Full length in video frames at DCP rate"), c.frames_round (frc.dcp)));

	if (stream) {
		p.push_back (
			UserProperty (
				_("Length"),
				_("Full length in audio frames at DCP rate"),
				c.frames_round (resampled_audio_frame_rate ())
				)
			);
	}
}

void
AudioContent::set_audio_video_frame_rate (double r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_video_frame_rate = r;
	}

	signal_changed (AudioContentProperty::AUDIO_VIDEO_FRAME_RATE);
}

double
AudioContent::audio_video_frame_rate () const
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_audio_video_frame_rate) {
			return _audio_video_frame_rate.get ();
		}
	}

	/* No frame rate specified, so assume this content has been
	   prepared for any concurrent video content.
	*/
	return film()->active_frame_rate_change(position()).source;
}

AudioContent::set_streams (vector<AudioStreamPtr> streams)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams = streams;
	}

	_parent->signal_changed (AudioContentProperty::AUDIO_STREAMS);
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
	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams.push_back (stream);
	}

	_parent->signal_changed (AudioContentProperty::AUDIO_STREAMS);
}

void
AudioContent::set_stream (AudioStreamPtr stream)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_streams.clear ();
		_streams.push_back (stream);
	}

	_parent->signal_changed (AudioContentProperty::AUDIO_STREAMS);
}
