/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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
#include "video_encoding.h"


using std::string;


string
video_encoding_to_string(VideoEncoding encoding)
{
	switch (encoding) {
	case VideoEncoding::JPEG2000:
		return "jpeg2000";
	case VideoEncoding::MPEG2:
		return "mpeg2";
	case VideoEncoding::COUNT:
		DCPOMATIC_ASSERT(false);
	}

	DCPOMATIC_ASSERT(false);
}


VideoEncoding
string_to_video_encoding(string const& encoding)
{
	if (encoding == "jpeg2000") {
		return VideoEncoding::JPEG2000;
	}

	if (encoding == "mpeg2") {
		return VideoEncoding::MPEG2;
	}

	DCPOMATIC_ASSERT(false);
}

