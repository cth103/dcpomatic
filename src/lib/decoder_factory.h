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

#ifndef DVDOMATIC_DECODER_FACTORY_H
#define DVDOMATIC_DECODER_FACTORY_H

/** @file  src/decoder_factory.h
 *  @brief A method to create appropriate decoders for some content.
 */

#include "options.h"

class Film;
class Job;
class VideoDecoder;
class AudioDecoder;

struct Decoders {
	Decoders () {}
	
	Decoders (boost::shared_ptr<VideoDecoder> v, boost::shared_ptr<AudioDecoder> a)
		: video (v)
		, audio (a)
	{}

	boost::shared_ptr<VideoDecoder> video;
	boost::shared_ptr<AudioDecoder> audio;
};

extern Decoders decoder_factory (
	boost::shared_ptr<Film>, DecodeOptions, Job *
	);

#endif
