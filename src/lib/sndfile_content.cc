/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "film.h"
#include "compose.hpp"
#include "job.h"
#include "util.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using std::cout;
using boost::shared_ptr;
using boost::lexical_cast;

SndfileContent::SndfileContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, AudioContent (f, p)
	, _audio_channels (0)
	, _audio_length (0)
	, _audio_frame_rate (0)
{

}

SndfileContent::SndfileContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node, int version)
	: Content (f, node)
	, AudioContent (f, node)
	, _audio_mapping (node->node_child ("AudioMapping"), version)
{
	_audio_channels = node->number_child<int> ("AudioChannels");
	_audio_length = node->number_child<AudioContent::Frame> ("AudioLength");
	_audio_frame_rate = node->number_child<int> ("AudioFrameRate");
}

string
SndfileContent::summary () const
{
	/* Get the string() here so that the name does not have quotes around it */
	return String::compose (_("%1 [audio]"), path_summary ());
}

string
SndfileContent::technical_summary () const
{
	return Content::technical_summary() + " - "
		+ AudioContent::technical_summary ()
		+ " - sndfile";
}

string
SndfileContent::information () const
{
	if (_audio_frame_rate == 0) {
		return "";
	}
	
	stringstream s;

	s << String::compose (
		_("%1 channels, %2kHz, %3 samples"),
		audio_channels(),
		content_audio_frame_rate() / 1000.0,
		audio_length()
		);
	
	return s.str ();
}

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}

void
SndfileContent::examine (shared_ptr<Job> job)
{
	job->set_progress_unknown ();
	Content::examine (job);

	shared_ptr<const Film> film = _film.lock ();
	assert (film);

	SndfileDecoder dec (film, shared_from_this());

	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_channels = dec.audio_channels ();
		_audio_length = dec.audio_length ();
		_audio_frame_rate = dec.audio_frame_rate ();
	}

	signal_changed (AudioContentProperty::AUDIO_CHANNELS);
	signal_changed (AudioContentProperty::AUDIO_LENGTH);
	signal_changed (AudioContentProperty::AUDIO_FRAME_RATE);

	{
		boost::mutex::scoped_lock lm (_mutex);
		/* XXX: do this in signal_changed...? */
		_audio_mapping = AudioMapping (_audio_channels);
		_audio_mapping.make_default ();
	}
	
	signal_changed (AudioContentProperty::AUDIO_MAPPING);
}

void
SndfileContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Sndfile");
	Content::as_xml (node);
	AudioContent::as_xml (node);

	node->add_child("AudioChannels")->add_child_text (lexical_cast<string> (audio_channels ()));
	node->add_child("AudioLength")->add_child_text (lexical_cast<string> (audio_length ()));
	node->add_child("AudioFrameRate")->add_child_text (lexical_cast<string> (content_audio_frame_rate ()));
	_audio_mapping.as_xml (node->add_child("AudioMapping"));
}

Time
SndfileContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);

	OutputAudioFrame const len = audio_length() * output_audio_frame_rate() / content_audio_frame_rate ();
	
	/* XXX: this depends on whether, alongside this audio, we are running video slower or faster than
	   it should be.  The calculation above works out the output audio frames assuming that we are just
	   resampling the audio: it would be incomplete if, for example, we were running this audio alongside
	   25fps video that was being run at 24fps.
	*/
	
	return film->audio_frames_to_time (len);
}

int
SndfileContent::output_audio_frame_rate () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	return film->audio_frame_rate ();
}

void
SndfileContent::set_audio_mapping (AudioMapping m)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_mapping = m;
	}

	signal_changed (AudioContentProperty::AUDIO_MAPPING);
}
