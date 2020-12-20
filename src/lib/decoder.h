/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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

/** @file  src/decoder.h
 *  @brief Decoder class.
 */

#ifndef DCPOMATIC_DECODER_H
#define DCPOMATIC_DECODER_H

#include "types.h"
#include "film.h"
#include "font_data.h"
#include "dcpomatic_time.h"
#include "weak_film.h"
#include <boost/utility.hpp>

class Decoded;
class VideoDecoder;
class AudioDecoder;
class TextDecoder;
class AtmosDecoder;
class DecoderPart;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder : public boost::noncopyable, public WeakConstFilm
{
public:
	Decoder (boost::weak_ptr<const Film> film);
	virtual ~Decoder () {}

	boost::shared_ptr<VideoDecoder> video;
	boost::shared_ptr<AudioDecoder> audio;
	std::list<boost::shared_ptr<TextDecoder> > text;
	boost::shared_ptr<AtmosDecoder> atmos;

	boost::shared_ptr<TextDecoder> only_text () const;

	/** Do some decoding and perhaps emit video, audio or subtitle data.
	 *  @return true if this decoder will emit no more data unless a seek() happens.
	 */
	virtual bool pass () = 0;
	virtual void seek (dcpomatic::ContentTime time, bool accurate);

	virtual dcpomatic::ContentTime position () const;

	virtual std::vector<dcpomatic::FontData> fonts () const {
		return std::vector<dcpomatic::FontData>();
	}
};

#endif
