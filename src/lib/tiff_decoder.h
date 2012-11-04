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

/** @file src/tiff_decoder.h
 *  @brief A decoder which reads a numbered set of TIFF files, one per frame.
 */

#ifndef DVDOMATIC_TIFF_DECODER_H
#define DVDOMATIC_TIFF_DECODER_H

#include <vector>
#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include "util.h"
#include "decoder.h"

class Job;
class FilmState;
class Options;
class Image;

/** @class TIFFDecoder.
 *  @brief A decoder which reads a numbered set of TIFF files, one per frame.
 */
class TIFFDecoder : public Decoder
{
public:
	TIFFDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *, bool);

	/* Methods to query our input video */
	float frames_per_second () const;
	Size native_size () const;
	int audio_channels () const;
	int audio_sample_rate () const;
	AVSampleFormat audio_sample_format () const;
	int64_t audio_channel_layout () const;
	bool has_subtitles () const {
		return false;
	}

private:
	bool pass ();
	PixelFormat pixel_format () const;
	int time_base_numerator () const;
	int time_base_denominator () const;
	int sample_aspect_ratio_numerator () const;
	int sample_aspect_ratio_denominator () const;
	
	std::string file_path (std::string) const;
	std::list<std::string> _files;
	std::list<std::string>::iterator _iter;
};

#endif
