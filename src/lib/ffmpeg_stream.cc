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
	, _id(node->optional_number_child<int>("Id"))
	, _index(node->optional_number_child<int>("Index"))
{

}

void
FFmpegStream::as_xml(xmlpp::Element* root) const
{
	cxml::add_text_child(root, "Name", name);
	if (_id) {
		cxml::add_text_child(root, "Id", fmt::to_string(*_id));
	}
	if (_index) {
		cxml::add_text_child(root, "Index", fmt::to_string(*_index));
	}
}

bool
FFmpegStream::uses_index(AVFormatContext const * fc, int index) const
{
	DCPOMATIC_ASSERT(static_cast<bool>(_id) || static_cast<bool>(_index));

	if (_id) {
		return fc->streams[index]->id == *_id;
	} else {
		return *_index == index;
	}
}

AVStream *
FFmpegStream::stream(AVFormatContext const * fc) const
{
	DCPOMATIC_ASSERT(static_cast<bool>(_id) || static_cast<bool>(_index));

	if (_id) {
		size_t i = 0;
		while (i < fc->nb_streams) {
			if (fc->streams[i]->id == *_id) {
				return fc->streams[i];
			}
			++i;
		}

		DCPOMATIC_ASSERT(false);
		return 0;
	} else {
		return fc->streams[*_index];
	}
}

int
FFmpegStream::index(AVFormatContext const * fc) const
{
	DCPOMATIC_ASSERT(static_cast<bool>(_id) || static_cast<bool>(_index));

	if (_id) {
		size_t i = 0;
		while (i < fc->nb_streams) {
			if (fc->streams[i]->id == *_id) {
				return i;
			}
			++i;
		}

		DCPOMATIC_ASSERT(false);
		return 0;
	} else {
		return *_index;
	}
}


string
FFmpegStream::technical_summary() const
{
	DCPOMATIC_ASSERT(static_cast<bool>(_id) || static_cast<bool>(_index));

	if (_id) {
		return fmt::format("id {}", *_id);
	} else {
		return fmt::format("index {}", *_index);
	}
}


string
FFmpegStream::identifier() const
{
	if (_id) {
		return fmt::to_string(*_id);
	} else {
		return fmt::to_string(*_index);
	}
}

