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

/** @file  src/transcoder.h
 *  @brief A class which takes a Film and some Options, then uses those to transcode the film.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

#include "decoder_factory.h"

class Film;
class Job;
class Encoder;
class Matcher;
class VideoFilter;
class Gain;
class VideoDecoder;
class AudioDecoder;
class DelayLine;

/** @class Transcoder
 *  @brief A class which takes a Film and some Options, then uses those to transcode the film.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */
class Transcoder
{
public:
	Transcoder (
		boost::shared_ptr<Film> f,
		DecodeOptions o,
		Job* j,
		boost::shared_ptr<Encoder> e
		);

	void go ();

	boost::shared_ptr<VideoDecoder> video_decoder () const {
		return _decoders.video;
	}

protected:
	/** A Job that is running this Transcoder, or 0 */
	Job* _job;
	/** The encoder that we will use */
	boost::shared_ptr<Encoder> _encoder;
	/** The decoders that we will use */
	Decoders _decoders;
	boost::shared_ptr<Matcher> _matcher;
	boost::shared_ptr<DelayLine> _delay_line;
	boost::shared_ptr<Gain> _gain;
};
