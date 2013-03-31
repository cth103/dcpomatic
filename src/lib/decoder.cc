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

#include <iostream>
#include "film.h"
#include "exceptions.h"
#include "util.h"
#include "decoder.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

/** @param f Film.
 *  @param o Decode options.
 */
Decoder::Decoder (shared_ptr<const Film> f)
	: _film (f)
{
	_film_connection = f->Changed.connect (bind (&Decoder::film_changed, this, _1));
}

/** Seek to a position as a source timestamp in seconds.
 *  @return true on error.
 */
bool
Decoder::seek (double)
{
	throw DecodeError (N_("decoder does not support seek"));
}

/** Seek so that the next frame we will produce is the same as the last one.
 *  @return true on error.
 */
bool
Decoder::seek_to_last ()
{
	throw DecodeError (N_("decoder does not support seek"));
}
