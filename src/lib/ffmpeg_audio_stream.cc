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


#include "ffmpeg_audio_stream.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <libcxml/cxml.h>
#include <fmt/format.h>


using std::string;
using boost::optional;
using namespace dcpomatic;


FFmpegAudioStream::FFmpegAudioStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
	, AudioStream (
		node->number_child<int>("FrameRate"),
		node->optional_number_child<Frame>("Length").get_value_or(0),
		AudioMapping(node->node_child("Mapping"), version),
		node->optional_number_child<int>("BitDepth")
		)
{
	optional<ContentTime::Type> const f = node->optional_number_child<ContentTime::Type>("FirstAudio");
	if (f) {
		first_audio = ContentTime(f.get());
	}
	codec_name = node->optional_string_child("CodecName");
}


void
FFmpegAudioStream::as_xml(xmlpp::Element* root) const
{
	FFmpegStream::as_xml (root);
	cxml::add_text_child(root, "FrameRate", fmt::to_string(frame_rate()));
	cxml::add_text_child(root, "Length", fmt::to_string(length()));
	mapping().as_xml(cxml::add_child(root, "Mapping"));
	if (first_audio) {
		cxml::add_text_child(root, "FirstAudio", fmt::to_string(first_audio.get().get()));
	}
	if (codec_name) {
		cxml::add_text_child(root, "CodecName", codec_name.get());
	}
	if (bit_depth()) {
		cxml::add_text_child(root, "BitDepth", fmt::to_string(bit_depth().get()));
	}
}
