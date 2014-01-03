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
#include "decoded.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;

/** @param f Film.
 *  @param o Decode options.
 */
Decoder::Decoder (shared_ptr<const Film> f)
	: _film (f)
	, _done (false)
{

}

shared_ptr<Decoded>
Decoder::peek ()
{
	while (!_done && _pending.empty ()) {
		_done = pass ();
	}

	if (_done && _pending.empty ()) {
		return shared_ptr<Decoded> ();
	}

	return _pending.front ();
}

void
Decoder::consume ()
{
	if (!_pending.empty ()) {
		_pending.pop_front ();
	}
}

void
Decoder::seek (ContentTime, bool)
{
	_pending.clear ();
	_done = false;
}
