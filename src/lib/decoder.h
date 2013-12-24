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

class Film;
class Decoded;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder : public boost::noncopyable
{
public:
	Decoder (boost::shared_ptr<const Film>);
	virtual ~Decoder () {}

	/** Seek so that the next get_*() will yield the next thing
	 *  (video/sound frame, subtitle etc.) at or after the requested
	 *  time.  Pass accurate = true to try harder to get close to
	 *  the request.
	 */
	virtual void seek (ContentTime time, bool accurate);
	
	boost::shared_ptr<Decoded> peek ();
	void consume ();

protected:

	/** Perform one decode pass of the content, which may or may not
	 *  result in a complete quantum (Decoded object) of decoded stuff
	 *  being made ready.
	 *  @return true if the decoder is done (i.e. no more data will be
	 *  produced by any future calls to pass() without a seek() first).
	 */
	virtual bool pass () = 0;
	virtual void flush () {};
	
	/** The Film that we are decoding in */
	boost::weak_ptr<const Film> _film;

	std::list<boost::shared_ptr<Decoded> > _pending;
};

#endif
