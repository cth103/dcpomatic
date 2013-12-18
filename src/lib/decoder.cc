/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/decoder.cc
 *  @brief Parent class for decoders of content.
 */

#include "film.h"
#include "decoder.h"

#include "i18n.h"

using boost::shared_ptr;

/** @param f Film.
 *  @param o Decode options.
 */
Decoder::Decoder (shared_ptr<const Film> f)
	: _film (f)
{

}

shared_ptr<Decoded>
Decoder::peek ()
{
	while (_pending.empty () && !pass ()) {}

	if (_pending.empty ()) {
		return shared_ptr<Decoded> ();
	}

	return _pending.front ();
}

shared_ptr<Decoded>
Decoder::get ()
{
	shared_ptr<Decoded> d = peek ();
	if (d) {
		_pending.pop_front ();
	}

	return d;
}

void
Decoder::seek (ContentTime, bool)
{
	_pending.clear ();
}
