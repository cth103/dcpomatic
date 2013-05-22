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

#include <vector>
#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include "video_source.h"
#include "audio_source.h"
#include "film.h"

class Image;
class Log;
class DelayLine;
class TimedSubtitle;
class Subtitle;
class FilterGraph;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder
{
public:
	Decoder (boost::shared_ptr<const Film>);
	virtual ~Decoder () {}

	/** Perform one decode pass of the content, which may or may not
	 *  cause the object to emit some data.
	 */
	virtual void pass () = 0;

	/** Seek this decoder to as close as possible to some time,
	 *  expressed relative to our source's start.
	 *  @param t Time.
	 */
	virtual void seek (Time t) {}

	/** Seek back one video frame */
	virtual void seek_back () {}

	/** Seek forward one video frame */
	virtual void seek_forward () {}

	/** @return Approximate time of the next content that we will emit,
	 *  expressed relative to the start of our source.
	 */
	virtual Time next () const = 0;

protected:

	/** The Film that we are decoding in */
	boost::weak_ptr<const Film> _film;

private:
	/** This will be called when our Film emits Changed */
	virtual void film_changed (Film::Property) {}

	/** Connection to our Film */
	boost::signals2::scoped_connection _film_connection;
};

#endif
