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

void
Decoder::maybe_seek (ContentTime time, bool accurate)
{
	if (!_position) {
		/* A seek has just happened */
		return;
	}

	if (time >= *_position && time < (*_position + ContentTime::from_seconds(1))) {
		/* No need to seek: caller should just pass() */
		return;
	}

	_position.reset ();
	seek (time, accurate);
}
