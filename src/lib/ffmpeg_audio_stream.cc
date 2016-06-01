/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_audio_stream.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>

using std::string;
using boost::optional;

FFmpegAudioStream::FFmpegAudioStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
	, AudioStream (
		node->number_child<int> ("FrameRate"),
		node->optional_number_child<Frame>("Length").get_value_or (0),
		AudioMapping (node->node_child ("Mapping"), version)
		)
{
	optional<ContentTime::Type> const f = node->optional_number_child<ContentTime::Type> ("FirstAudio");
	if (f) {
		first_audio = ContentTime (f.get ());
	}
	codec_name = node->optional_string_child("CodecName");
}

void
FFmpegAudioStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);
	root->add_child("FrameRate")->add_child_text (raw_convert<string> (frame_rate ()));
	root->add_child("Length")->add_child_text (raw_convert<string> (length ()));
	mapping().as_xml (root->add_child("Mapping"));
	if (first_audio) {
		root->add_child("FirstAudio")->add_child_text (raw_convert<string> (first_audio.get().get ()));
	}
	if (codec_name) {
		root->add_child("CodecName")->add_child_text (codec_name.get());
	}
}
