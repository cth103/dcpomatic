/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include "audio_content.h"
#include "analyse_audio_job.h"
#include "job_manager.h"
#include "film.h"
#include "exceptions.h"
#include "config.h"
#include "frame_rate_change.h"
#include "audio_processor.h"

#include "i18n.h"

using std::string;
using std::cout;
using std::vector;
using std::stringstream;
using std::fixed;
using std::setprecision;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

int const AudioContentProperty::AUDIO_CHANNELS = 200;
int const AudioContentProperty::AUDIO_LENGTH = 201;
int const AudioContentProperty::AUDIO_FRAME_RATE = 202;
int const AudioContentProperty::AUDIO_GAIN = 203;
int const AudioContentProperty::AUDIO_DELAY = 204;
int const AudioContentProperty::AUDIO_MAPPING = 205;
int const AudioContentProperty::AUDIO_PROCESSOR = 206;

AudioContent::AudioContent (shared_ptr<const Film> f)
	: Content (f)
	, _audio_gain (0)
	, _audio_delay (Config::instance()->default_audio_delay ())
	, _audio_processor (0)
{

}

AudioContent::AudioContent (shared_ptr<const Film> f, DCPTime s)
	: Content (f, s)
	, _audio_gain (0)
	, _audio_delay (Config::instance()->default_audio_delay ())
	, _audio_processor (0)
{

}

AudioContent::AudioContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _audio_gain (0)
	, _audio_delay (Config::instance()->default_audio_delay ())
	, _audio_processor (0)
{

}

AudioContent::AudioContent (shared_ptr<const Film> f, cxml::ConstNodePtr node)
	: Content (f, node)
	, _audio_processor (0)
{
	_audio_gain = node->number_child<float> ("AudioGain");
	_audio_delay = node->number_child<int> ("AudioDelay");
	if (node->optional_string_child ("AudioProcessor")) {
		_audio_processor = AudioProcessor::from_id (node->string_child ("AudioProcessor"));
	}
}

AudioContent::AudioContent (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: Content (f, c)
{
	shared_ptr<AudioContent> ref = dynamic_pointer_cast<AudioContent> (c[0]);
	assert (ref);
	
	for (size_t i = 0; i < c.size(); ++i) {
		shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (c[i]);

		if (ac->audio_gain() != ref->audio_gain()) {
			throw JoinError (_("Content to be joined must have the same audio gain."));
		}

		if (ac->audio_delay() != ref->audio_delay()) {
			throw JoinError (_("Content to be joined must have the same audio delay."));
		}
	}

	_audio_gain = ref->audio_gain ();
	_audio_delay = ref->audio_delay ();
	_audio_processor = ref->audio_processor ();
}

void
AudioContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("AudioGain")->add_child_text (raw_convert<string> (_audio_gain));
	node->add_child("AudioDelay")->add_child_text (raw_convert<string> (_audio_delay));
	if (_audio_processor) {
		node->add_child("AudioProcessor")->add_child_text (_audio_processor->id ());
	}
}


void
AudioContent::set_audio_gain (double g)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_gain = g;
	}
	
	signal_changed (AudioContentProperty::AUDIO_GAIN);
}

void
AudioContent::set_audio_delay (int d)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_delay = d;
	}
	
	signal_changed (AudioContentProperty::AUDIO_DELAY);
}

void
AudioContent::set_audio_processor (AudioProcessor const * p)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_processor = p;
	}

	/* The channel count might have changed, so reset the mapping */
	AudioMapping m (processed_audio_channels ());
	m.make_default ();
	set_audio_mapping (m);

	signal_changed (AudioContentProperty::AUDIO_PROCESSOR);
}

boost::signals2::connection
AudioContent::analyse_audio (boost::function<void()> finished)
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	shared_ptr<AnalyseAudioJob> job (new AnalyseAudioJob (film, dynamic_pointer_cast<AudioContent> (shared_from_this())));
	boost::signals2::connection c = job->Finished.connect (finished);
	JobManager::instance()->add (job);

	return c;
}

boost::filesystem::path
AudioContent::audio_analysis_path () const
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return boost::filesystem::path ();
	}

	boost::filesystem::path p = film->audio_analysis_dir ();
	p /= digest() + "_" + audio_mapping().digest();
	return p;
}

string
AudioContent::technical_summary () const
{
	return String::compose (
		"audio: channels %1, length %2, content rate %3, resampled rate %4",
		audio_channels(),
		audio_length().seconds(),
		audio_frame_rate(),
		resampled_audio_frame_rate()
		);
}

void
AudioContent::set_audio_mapping (AudioMapping)
{
	signal_changed (AudioContentProperty::AUDIO_MAPPING);
}

/** @return the frame rate that this content should be resampled to in order
 *  that it is in sync with the active video content at its start time.
 */
int
AudioContent::resampled_audio_frame_rate () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_frame_rate (audio_frame_rate ());

	FrameRateChange frc = film->active_frame_rate_change (position ());

	/* Compensate if the DCP is being run at a different frame rate
	   to the source; that is, if the video is run such that it will
	   look different in the DCP compared to the source (slower or faster).
	*/

	if (frc.change_speed) {
		t /= frc.speed_up;
	}

	return rint (t);
}

int
AudioContent::processed_audio_channels () const
{
	if (!audio_processor ()) {
		return audio_channels ();
	}

	return audio_processor()->out_channels (audio_channels ());
}

string
AudioContent::processing_description () const
{
	stringstream d;
	
	if (audio_frame_rate() != resampled_audio_frame_rate ()) {
		stringstream from;
		from << fixed << setprecision(3) << (audio_frame_rate() / 1000.0);
		stringstream to;
		to << fixed << setprecision(3) << (resampled_audio_frame_rate() / 1000.0);

		d << String::compose (_("Audio will be resampled from %1kHz to %2kHz."), from.str(), to.str());
	} else {
		d << _("Audio will not be resampled.");
	}

	return d.str ();
}

