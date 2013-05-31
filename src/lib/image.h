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

#ifndef DCPOMATIC_IMAGE_H
#define DCPOMATIC_IMAGE_H

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#include "util.h"

class Scaler;
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
	Image (AVPixelFormat p)
		: _pixel_format (p)
	{}
	
	virtual ~Image () {}

	/** @return Array of pointers to arrays of the component data */
	virtual uint8_t ** data () const = 0;

	/** @return Array of sizes of the data in each line, in bytes (without any alignment padding bytes) */
	virtual int * line_size () const = 0;

	/** @return Array of strides for each line (including any alignment padding bytes) */
	virtual int * stride () const = 0;

	/** @return Size of the image, in pixels */
	virtual libdcp::Size size () const = 0;

	virtual bool aligned () const = 0;

	int components () const;
	int lines (int) const;

	boost::shared_ptr<Image> scale_and_convert_to_rgb (libdcp::Size, Scaler const *, bool) const;
	boost::shared_ptr<Image> scale (libdcp::Size, Scaler const *, bool aligned) const;
	boost::shared_ptr<Image> post_process (std::string, bool aligned) const;
	void alpha_blend (boost::shared_ptr<const Image> image, Position pos);
	void copy (boost::shared_ptr<const Image> image, Position pos);
	boost::shared_ptr<Image> crop (Crop c, bool aligned) const;
	
	void make_black ();

	void read_from_socket (boost::shared_ptr<Socket>);
	void write_to_socket (boost::shared_ptr<Socket>) const;
	
	AVPixelFormat pixel_format () const {
		return _pixel_format;
	}

protected:
	virtual void swap (Image &);
	float bytes_per_pixel (int) const;

	friend class pixel_formats_test;

private:
	void yuv_16_black (uint16_t);
	static uint16_t swap_16 (uint16_t);
	
	AVPixelFormat _pixel_format; ///< FFmpeg's way of describing the pixel format of this Image
};

/** @class SimpleImage
 *  @brief An Image for which memory is allocated using a `simple' av_malloc().
 */
class SimpleImage : public Image
{
public:
	SimpleImage (AVPixelFormat, libdcp::Size, bool);
	SimpleImage (AVFrame *);
	SimpleImage (SimpleImage const &);
	SimpleImage (boost::shared_ptr<const Image>);
	SimpleImage& operator= (SimpleImage const &);
	~SimpleImage ();

	uint8_t ** data () const;
	int * line_size () const;
	int * stride () const;
	libdcp::Size size () const;
	bool aligned () const;

protected:
	void allocate ();
	void swap (SimpleImage &);
	
private:
	libdcp::Size _size; ///< size in pixels
	uint8_t** _data; ///< array of pointers to components
	int* _line_size; ///< array of sizes of the data in each line, in pixels (without any alignment padding bytes)
	int* _stride; ///< array of strides for each line (including any alignment padding bytes)
	bool _aligned;
};

class RGBPlusAlphaImage : public SimpleImage
{
public:
	RGBPlusAlphaImage (boost::shared_ptr<const Image>);
	~RGBPlusAlphaImage ();

	uint8_t* alpha () const {
		return _alpha;
	}
	
private:
	uint8_t* _alpha;
};

#endif
