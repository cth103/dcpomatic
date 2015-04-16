/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "single_stream_audio_content.h"
#include "audio_examiner.h"
#include "film.h"
#include "raw_convert.h"

using std::string;
using std::cout;
using boost::shared_ptr;

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f)
	: Content (f)
	, AudioContent (f)
	, _audio_channels (0)
	, _audio_length (0)
	, _audio_frame_rate (0)
{

}

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, AudioContent (f, p)
	, _audio_channels (0)
	, _audio_length (0)
	, _audio_frame_rate (0)
{

}

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
	, AudioContent (f, node)
	, _audio_mapping (node->node_child ("AudioMapping"), version)
{
	_audio_channels = node->number_child<int> ("AudioChannels");
	_audio_length = ContentTime (node->number_child<ContentTime::Type> ("AudioLength"));
	_audio_frame_rate = node->number_child<int> ("AudioFrameRate");
}

void
SingleStreamAudioContent::set_audio_mapping (AudioMapping m)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_mapping = m;
	}

	AudioContent::set_audio_mapping (m);
}


void
SingleStreamAudioContent::as_xml (xmlpp::Node* node) const
{
	AudioContent::as_xml (node);
	node->add_child("AudioChannels")->add_child_text (raw_convert<string> (audio_channels ()));
	node->add_child("AudioLength")->add_child_text (raw_convert<string> (audio_length().get ()));
	node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio_frame_rate ()));
	_audio_mapping.as_xml (node->add_child("AudioMapping"));
}

void
SingleStreamAudioContent::take_from_audio_examiner (shared_ptr<AudioExaminer> examiner)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_channels = examiner->audio_channels ();
		_audio_length = examiner->audio_length ();
		_audio_frame_rate = examiner->audio_frame_rate ();
	}

	signal_changed (AudioContentProperty::AUDIO_CHANNELS);
	signal_changed (AudioContentProperty::AUDIO_LENGTH);
	signal_changed (AudioContentProperty::AUDIO_FRAME_RATE);

	int const p = processed_audio_channels ();

	{
		boost::mutex::scoped_lock lm (_mutex);
		/* XXX: do this in signal_changed...? */
		_audio_mapping = AudioMapping (p);
		_audio_mapping.make_default ();
	}
	
	signal_changed (AudioContentProperty::AUDIO_MAPPING);
}
