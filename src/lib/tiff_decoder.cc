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

/** @file src/tiff_decoder.cc
 *  @brief A decoder which reads a numbered set of TIFF files, one per frame.
 */

#include <vector>
#include <string>
#include <iostream>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <tiffio.h>
extern "C" {
#include <libavformat/avformat.h>
}
#include "util.h"
#include "tiff_decoder.h"
#include "exceptions.h"
#include "image.h"
#include "options.h"
#include "film.h"

using namespace std;
using namespace boost;

/** @param f Our Film.
 *  @param o Options.
 *  @param j Job that we are associated with, or 0.
 *  @param minimal true to do the bare minimum of work; just run through the content.  Useful for acquiring
 *  accurate frame counts as quickly as possible.  This generates no video or audio output.
 *  @param ignore_length Ignore the content's claimed length when computing progress.
 */
TIFFDecoder::TIFFDecoder (boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j, bool minimal, bool ignore_length)
	: Decoder (f, o, j, minimal, ignore_length)
{
	string const dir = _film->content_path ();
	
	if (!filesystem::is_directory (dir)) {
		throw DecodeError ("TIFF content must be in a directory");
	}

	for (filesystem::directory_iterator i = filesystem::directory_iterator (dir); i != filesystem::directory_iterator(); ++i) {
		/* Aah, the sweet smell of progress */
#if BOOST_FILESYSTEM_VERSION == 3
		string const ext = filesystem::path(*i).extension().string();
		string const l = filesystem::path(*i).leaf().generic_string();
#else
		string const ext = filesystem::path(*i).extension();
		string const l = i->leaf ();
#endif
		if (ext == ".tif" || ext == ".tiff") {
			_files.push_back (l);
		}
	}

	_files.sort ();

	_iter = _files.begin ();

}

float
TIFFDecoder::frames_per_second () const
{
	/* We don't know */
	return 0;
}

Size
TIFFDecoder::native_size () const
{
	if (_files.empty ()) {
		throw DecodeError ("no TIFF files found");
	}
	
	TIFF* t = TIFFOpen (file_path (_files.front()).c_str (), "r");
	if (t == 0) {
		throw DecodeError ("could not open TIFF file");
	}

	uint32_t width;
	TIFFGetField (t, TIFFTAG_IMAGEWIDTH, &width);
	uint32_t height;
	TIFFGetField (t, TIFFTAG_IMAGELENGTH, &height);

	return Size (width, height);
}

int
TIFFDecoder::audio_channels () const
{
	/* No audio */
	return 0;
}

int
TIFFDecoder::audio_sample_rate () const
{
	return 0;
}

AVSampleFormat
TIFFDecoder::audio_sample_format () const
{
	return AV_SAMPLE_FMT_NONE;
}


int64_t
TIFFDecoder::audio_channel_layout () const
{
	return 0;
}

bool
TIFFDecoder::do_pass ()
{
	if (_iter == _files.end ()) {
		return true;
	}

	TIFF* t = TIFFOpen (file_path (*_iter).c_str (), "r");
	if (t == 0) {
		throw DecodeError ("could not open TIFF file");
	}

	uint32_t width;
	TIFFGetField (t, TIFFTAG_IMAGEWIDTH, &width);
	uint32_t height;
	TIFFGetField (t, TIFFTAG_IMAGELENGTH, &height);

	int const num_pixels = width * height;
	uint32_t * raster = (uint32_t *) _TIFFmalloc (num_pixels * sizeof (uint32_t));
	if (raster == 0) {
		throw DecodeError ("could not allocate memory to decode TIFF file");
	}

	if (TIFFReadRGBAImage (t, width, height, raster, 0) < 0) {
		throw DecodeError ("could not read TIFF data");
	}

	RGBFrameImage image (Size (width, height));

	uint8_t* p = image.data()[0];
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint32_t const i = (height - y - 1) * width + x;
			*p++ = raster[i] & 0xff;
			*p++ = (raster[i] & 0xff00) >> 8;
			*p++ = (raster[i] & 0xff0000) >> 16;
		}
	}

	_TIFFfree (raster);
	TIFFClose (t);

	process_video (image.frame ());

	++_iter;
	return false;
}

/** @param file name within our content directory
 *  @return full path to the file
 */
string
TIFFDecoder::file_path (string f) const
{
	stringstream s;
	s << _film->content_path() << "/" << f;
	return _film->file (s.str ());
}

PixelFormat
TIFFDecoder::pixel_format () const
{
	return PIX_FMT_RGB24;
}

int
TIFFDecoder::time_base_numerator () const
{
	return dcp_frame_rate(_film->frames_per_second()).frames_per_second;
}


int
TIFFDecoder::time_base_denominator () const
{
	return 1;
}

int
TIFFDecoder::sample_aspect_ratio_numerator () const
{
	/* XXX */
	return 1;
}

int
TIFFDecoder::sample_aspect_ratio_denominator () const
{
	/* XXX */
	return 1;
}
