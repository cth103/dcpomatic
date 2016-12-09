/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include "decoder_part.h"
#include <iostream>

using std::cout;
using boost::optional;

void
Decoder::maybe_seek (optional<ContentTime> position, ContentTime time, bool accurate)
{
	if (position && (time >= position.get() && time < (position.get() + ContentTime::from_seconds(1)))) {
		/* No need to seek: caller should just pass() */
		return;
	}

	seek (time, accurate);
}
