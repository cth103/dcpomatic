/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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
using std::vector;
using boost::shared_ptr;

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f)
	: Content (f)
	, AudioContent (f)
{

}

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, AudioContent (f, p)
{

}

SingleStreamAudioContent::SingleStreamAudioContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
	, AudioContent (f, node)
	, _audio_stream (new AudioStream (node->number_child<int> ("AudioFrameRate"), AudioMapping (node->node_child ("AudioMapping"), version)))
{

}

void
SingleStreamAudioContent::as_xml (xmlpp::Node* node) const
{
	AudioContent::as_xml (node);
	node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio_stream()->frame_rate ()));
	audio_stream()->mapping().as_xml (node->add_child("AudioMapping"));
}

void
SingleStreamAudioContent::take_from_audio_examiner (shared_ptr<AudioExaminer> examiner)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_audio_stream.reset (new AudioStream (examiner->audio_frame_rate(), examiner->audio_channels ()));
		AudioMapping m = _audio_stream->mapping ();
		m.make_default ();
		_audio_stream->set_mapping (m);
	}

	signal_changed (AudioContentProperty::AUDIO_STREAMS);
}

vector<AudioStreamPtr>
SingleStreamAudioContent::audio_streams () const
{
	vector<AudioStreamPtr> s;
	s.push_back (_audio_stream);
	return s;
}
