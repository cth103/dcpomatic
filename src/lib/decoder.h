/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_time.h"
#include <boost/utility.hpp>

class Decoded;
class VideoDecoder;
class AudioDecoder;
class CaptionDecoder;
class DecoderPart;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 */
class Decoder : public boost::noncopyable
{
public:
	virtual ~Decoder () {}

	boost::shared_ptr<VideoDecoder> video;
	boost::shared_ptr<AudioDecoder> audio;
	boost::shared_ptr<CaptionDecoder> caption;

	/** Do some decoding and perhaps emit video, audio or subtitle data.
	 *  @return true if this decoder will emit no more data unless a seek() happens.
	 */
	virtual bool pass () = 0;
	virtual void seek (ContentTime time, bool accurate);

	ContentTime position () const;
};

#endif
