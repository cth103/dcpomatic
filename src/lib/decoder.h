/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

/** @file  src/decoder.h
 *  @brief Parent class for decoders of content.
 */

#ifndef DCPOMATIC_DECODER_H
#define DCPOMATIC_DECODER_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/utility.hpp>
#include "types.h"
#include "dcpomatic_time.h"

class Decoded;
class Film;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder : public boost::noncopyable
{
public:
	virtual ~Decoder () {}

protected:	
	/** Seek so that the next peek() will yield the next thing
	 *  (video/sound frame, subtitle etc.) at or after the requested
	 *  time.  Pass accurate = true to try harder to get close to
	 *  the request.
	 */
	virtual void seek (ContentTime time, bool accurate) = 0;
	virtual bool pass () = 0;
};

#endif
