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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "log.h"
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::map;
using boost::shared_ptr;
using boost::optional;

AudioDecoder::AudioDecoder (Decoder* parent, shared_ptr<const AudioContent> content, shared_ptr<Log> log)
	: DecoderPart (parent, log)
{
	BOOST_FOREACH (AudioStreamPtr i, content->streams ()) {
		_positions[i] = 0;
	}
}

void
AudioDecoder::emit (AudioStreamPtr stream, shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (ignore ()) {
		return;
	}

	if (_positions[stream] == 0) {
		_positions[stream] = time.frames_round (stream->frame_rate ());
	}

	Data (stream, ContentAudio (data, _positions[stream]));
	_positions[stream] += data->frames();
}

ContentTime
AudioDecoder::position () const
{
	optional<ContentTime> p;
	for (map<AudioStreamPtr, Frame>::const_iterator i = _positions.begin(); i != _positions.end(); ++i) {
		ContentTime const ct = ContentTime::from_frames (i->second, i->first->frame_rate ());
		if (!p || ct < *p) {
			p = ct;
		}
	}

	return p.get_value_or(ContentTime());
}

void
AudioDecoder::seek ()
{
	for (map<AudioStreamPtr, Frame>::iterator i = _positions.begin(); i != _positions.end(); ++i) {
		i->second = 0;
	}
}
