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

/** @file src/tiff_encoder.h
 *  @brief An encoder that writes TIFF files (and does nothing with audio).
 */

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <boost/filesystem.hpp>
#include <tiffio.h>
#include "tiff_encoder.h"
#include "film.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"

using namespace std;
using namespace boost;

/** @param s FilmState of the film that we are encoding.
 *  @param o Options.
 *  @param l Log.
 */
TIFFEncoder::TIFFEncoder (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Encoder (s, o, l)
{
	
}

void
TIFFEncoder::process_video (shared_ptr<Image> image, int frame)
{
	shared_ptr<Image> scaled = image->scale_and_convert_to_rgb (_opt->out_size, _opt->padding, _fs->scaler);
	string tmp_file = _opt->frame_out_path (frame, true);
	TIFF* output = TIFFOpen (tmp_file.c_str (), "w");
	if (output == 0) {
		throw CreateFileError (tmp_file);
	}
						
	TIFFSetField (output, TIFFTAG_IMAGEWIDTH, _opt->out_size.width);
	TIFFSetField (output, TIFFTAG_IMAGELENGTH, _opt->out_size.height);
	TIFFSetField (output, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField (output, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField (output, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField (output, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField (output, TIFFTAG_SAMPLESPERPIXEL, 3);
	
	if (TIFFWriteEncodedStrip (output, 0, scaled->data()[0], _opt->out_size.width * _opt->out_size.height * 3) == 0) {
		throw WriteFileError (tmp_file, 0);
	}

	TIFFClose (output);

	boost::filesystem::rename (tmp_file, _opt->frame_out_path (frame, false));
	frame_done (frame);
}
