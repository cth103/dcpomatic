/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_stream.h"
#include "dcpomatic_assert.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
extern "C" {
#include <libavformat/avformat.h>
}
DCPOMATIC_ENABLE_WARNINGS

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
	return fc->streams[index]->id == _id;
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

int
FFmpegStream::index (AVFormatContext const * fc) const
{
	size_t i = 0;
	while (i < fc->nb_streams) {
		if (fc->streams[i]->id == _id) {
			return i;
		}
		++i;
	}

	DCPOMATIC_ASSERT (false);
	return 0;
}
