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

#include "ffmpeg_audio_stream.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>

using std::string;

FFmpegAudioStream::FFmpegAudioStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
	, _frame_rate (node->number_child<int> ("FrameRate"))
	, _channels (node->number_child<int64_t> ("Channels"))
	, _mapping (node->node_child ("Mapping"), version)
{
	first_audio = node->optional_number_child<double> ("FirstAudio");
}

void
FFmpegAudioStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);
	root->add_child("FrameRate")->add_child_text (raw_convert<string> (_frame_rate));
	root->add_child("Channels")->add_child_text (raw_convert<string> (_channels));
	if (first_audio) {
		root->add_child("FirstAudio")->add_child_text (raw_convert<string> (first_audio.get().get()));
	}
	_mapping.as_xml (root->add_child("Mapping"));
}
