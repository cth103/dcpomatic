/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_time.h"
#include "film.h"
#include "font_data.h"
#include "types.h"
#include "weak_film.h"
#include <boost/utility.hpp>


class AtmosDecoder;
class AudioDecoder;
class Decoded;
class DecoderPart;
class TextDecoder;
class VideoDecoder;


/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder : public WeakConstFilm
{
public:
	Decoder (std::weak_ptr<const Film> film);
	virtual ~Decoder () {}

	Decoder (Decoder const&) = delete;
	Decoder& operator= (Decoder const&) = delete;

	std::shared_ptr<VideoDecoder> video;
	std::shared_ptr<AudioDecoder> audio;
	std::list<std::shared_ptr<TextDecoder>> text;
	std::shared_ptr<AtmosDecoder> atmos;

	std::shared_ptr<TextDecoder> only_text () const;

	/** Do some decoding and perhaps emit video, audio or subtitle data.
	 *  @return true if this decoder will emit no more data unless a seek() happens.
	 */
	virtual bool pass () = 0;
	virtual void seek (dcpomatic::ContentTime time, bool accurate);

	virtual dcpomatic::ContentTime position () const;

	virtual std::vector<dcpomatic::FontData> fonts () const {
		return {};
	}
};


#endif
