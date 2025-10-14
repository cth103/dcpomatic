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


#include "dcpomatic_assert.h"
#include "ffmpeg_stream.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
extern "C" {
#include <libavformat/avformat.h>
}
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>


using std::string;


FFmpegStream::FFmpegStream(cxml::ConstNodePtr node)
	: name(node->string_child("Name"))
	, _id(node->number_child<int>("Id"))
{

}

void
FFmpegStream::as_xml(xmlpp::Element* root) const
{
	cxml::add_text_child(root, "Name", name);
	cxml::add_text_child(root, "Id", fmt::to_string(_id));
}

bool
FFmpegStream::uses_index(AVFormatContext const * fc, int index) const
{
	return fc->streams[index]->id == _id;
}

AVStream *
FFmpegStream::stream(AVFormatContext const * fc) const
{
	size_t i = 0;
	while (i < fc->nb_streams) {
		if (fc->streams[i]->id == _id) {
			return fc->streams[i];
		}
		++i;
	}

	DCPOMATIC_ASSERT(false);
	return 0;
}

int
FFmpegStream::index(AVFormatContext const * fc) const
{
	size_t i = 0;
	while (i < fc->nb_streams) {
		if (fc->streams[i]->id == _id) {
			return i;
		}
		++i;
	}

	DCPOMATIC_ASSERT(false);
	return 0;
}


string
FFmpegStream::technical_summary() const
{
	return "id " + fmt::to_string(_id);
}


string
FFmpegStream::identifier() const
{
	return fmt::to_string(_id);
}

