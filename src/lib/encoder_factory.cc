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

/** @file  src/encoder_factory.cc
 *  @brief A method to create an appropriate encoder for some content.
 */

#include <boost/filesystem.hpp>
#include "j2k_wav_encoder.h"
#include "j2k_still_encoder.h"
#include "film_state.h"

using namespace std;
using namespace boost;

shared_ptr<Encoder>
encoder_factory (shared_ptr<const FilmState> fs, shared_ptr<const Options> o, Log* l)
{
	if (!filesystem::is_directory (fs->content_path()) && fs->content_type() == STILL) {
		return shared_ptr<Encoder> (new J2KStillEncoder (fs, o, l));
	}
	
	return shared_ptr<Encoder> (new J2KWAVEncoder (fs, o, l));
}
