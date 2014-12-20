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

#include "ffmpeg_stream.h"
#include "dcpomatic_assert.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
extern "C" {
#include <libavformat/avformat.h>
}

using std::string;
using dcp::raw_convert;

FFmpegStream::FFmpegStream (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
	, _id (node->number_child<int> ("Id"))
{

}

void
FFmpegStream::as_xml (xmlpp::Node* root) const
{
	root->add_child("Name")->add_child_text (name);
	root->add_child("Id")->add_child_text (raw_convert<string> (_id));
}

bool
FFmpegStream::uses_index (AVFormatContext const * fc, int index) const
{
	size_t i = 0;
	while (i < fc->nb_streams) {
		if (fc->streams[i]->id == _id) {
			return int (i) == index;
		}
		++i;
	}

	return false;
}

AVStream *
FFmpegStream::stream (AVFormatContext const * fc) const
{
	size_t i = 0;
	while (i < fc->nb_streams) {
		if (fc->streams[i]->id == _id) {
			return fc->streams[i];
		}
		++i;
	}

	DCPOMATIC_ASSERT (false);
	return 0;
}
