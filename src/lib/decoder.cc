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

#include "decoder.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "subtitle_decoder.h"
#include <boost/optional.hpp>
#include <iostream>

using std::cout;
using boost::optional;

/** @return Earliest time of content that the next pass() will emit */
ContentTime
Decoder::position () const
{
	optional<ContentTime> pos;

	if (video && (!pos || video->position() < *pos)) {
		pos = video->position();
	}

	if (audio && (!pos || audio->position() < *pos)) {
		pos = audio->position();
	}

	if (subtitle && (!pos || subtitle->position() < *pos)) {
		pos = subtitle->position();
	}

	return pos.get_value_or(ContentTime());
}

void
Decoder::seek (ContentTime, bool)
{
	if (audio) {
		audio->seek ();
	}
}
