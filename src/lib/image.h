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

/** @file src/image.h
 *  @brief A set of classes to describe video images.
 */

#ifndef DVDOMATIC_IMAGE_H
#define DVDOMATIC_IMAGE_H

#include <string>
#include <boost/shared_ptr.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#include "util.h"

class Scaler;
class RGBFrameImage;
class SimpleImage;

/** @class Image
 *  @brief Parent class for wrappers of some image, in some format, that
 *  can present a set of components and a size in pixels.
 *
 *  This class also has some conversion / processing methods.
 *
 *  The main point of this class (and its subclasses) is to abstract
 *  details of FFmpeg's memory management and varying data formats.
 */
class Image
{
public:
	Image (PixelFormat p)
		: _pixel_format (p)
	{}
	
	virtual ~Image () {}

	/** @return Array of pointers to arrays of the component data */
	virtual uint8_t ** data () const = 0;

	/** @return Array of sizes of each line, in pixels */
	virtual int * line_size () const = 0;

	/** @return Size of the image, in pixels */
	virtual Size size () const = 0;

	int components () const;
	int lines (int) const;
	boost::shared_ptr<Image> scale_and_convert_to_rgb (Size, int, Scaler const *) const;
	boost::shared_ptr<Image> scale (Size, Scaler const *) const;
	boost::shared_ptr<Image> post_process (std::string) const;
	
	void make_black ();
	
	PixelFormat pixel_format () const {
		return _pixel_format;
	}

private:
	PixelFormat _pixel_format; ///< FFmpeg's way of describing the pixel format of this Image
};

/** @class FilterBufferImage
 *  @brief An Image that is held in an AVFilterBufferRef.
 */
class FilterBufferImage : public Image
{
public:
	FilterBufferImage (PixelFormat, AVFilterBufferRef *);
	~FilterBufferImage ();

	uint8_t ** data () const;
	int * line_size () const;
	Size size () const;

private:
	AVFilterBufferRef* _buffer;
};

/** @class SimpleImage
 *  @brief An Image for which memory is allocated using a `simple' av_malloc().
 */
class SimpleImage : public Image
{
public:
	SimpleImage (PixelFormat, Size);
	~SimpleImage ();

	uint8_t ** data () const;
	int * line_size () const;
	Size size () const;
	
private:
	Size _size; ///< size in pixels
	uint8_t** _data; ///< array of pointers to components
	int* _line_size; ///< array of widths of each line, in bytes
};

/** @class RGBFrameImage
 *  @brief An RGB image that is held within an AVFrame.
 */
class RGBFrameImage : public Image
{
public:
	RGBFrameImage (Size);
	~RGBFrameImage ();

	uint8_t ** data () const;
	int * line_size () const;
	Size size () const;
	AVFrame * frame () const {
		return _frame;
	}

private:
	Size _size;
	AVFrame* _frame;
	uint8_t* _data;
};

#endif
