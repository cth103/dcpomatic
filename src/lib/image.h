/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file src/image.h
 *  @brief A set of classes to describe video images.
 */

#ifndef DCPOMATIC_IMAGE_H
#define DCPOMATIC_IMAGE_H

#include "position.h"
#include "position_image.h"
#include "types.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <dcp/array_data.h>
#include <dcp/colour_conversion.h>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

struct AVFrame;
class Socket;

class Image : public boost::enable_shared_from_this<Image>
{
public:
	Image (AVPixelFormat p, dcp::Size s, bool aligned);
	explicit Image (AVFrame *);
	explicit Image (Image const &);
	Image (boost::shared_ptr<const Image>, bool);
	Image& operator= (Image const &);
	~Image ();

	uint8_t * const * data () const;
	/** @return array of sizes of the data in each line, in bytes (not including any alignment padding) */
	int const * line_size () const;
	/** @return array of sizes of the data in each line, in bytes (including any alignment padding) */
	int const * stride () const;
	dcp::Size size () const;
	bool aligned () const;

	int planes () const;
	int vertical_factor (int) const;
	int horizontal_factor (int) const;
	dcp::Size sample_size (int) const;
	float bytes_per_pixel (int) const;

	boost::shared_ptr<Image> convert_pixel_format (dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, bool aligned, bool fast) const;
	boost::shared_ptr<Image> scale (dcp::Size out_size, dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, bool aligned, bool fast) const;
	boost::shared_ptr<Image> crop_scale_window (
		Crop crop, dcp::Size inter_size, dcp::Size out_size, dcp::YUVToRGB yuv_to_rgb, VideoRange video_range, AVPixelFormat out_format, bool aligned, bool fast
		) const;

	void make_black ();
	void make_transparent ();
	void alpha_blend (boost::shared_ptr<const Image> image, Position<int> pos);
	void copy (boost::shared_ptr<const Image> image, Position<int> pos);
	void fade (float);
	void video_range_to_full_range ();

	void read_from_socket (boost::shared_ptr<Socket>);
	void write_to_socket (boost::shared_ptr<Socket>) const;

	AVPixelFormat pixel_format () const {
		return _pixel_format;
	}

	size_t memory_used () const;

	dcp::ArrayData as_png () const;

	void png_error (char const * message);

	static boost::shared_ptr<const Image> ensure_aligned (boost::shared_ptr<const Image> image);

private:
	friend struct pixel_formats_test;

	void allocate ();
	void swap (Image &);
	void make_part_black (int x, int w);
	void yuv_16_black (uint16_t, bool);
	static uint16_t swap_16 (uint16_t);

	dcp::Size _size;
	AVPixelFormat _pixel_format; ///< FFmpeg's way of describing the pixel format of this Image
	uint8_t** _data; ///< array of pointers to components
	int* _line_size; ///< array of sizes of the data in each line, in bytes (without any alignment padding bytes)
	int* _stride; ///< array of strides for each line, in bytes (including any alignment padding bytes)
	bool _aligned;
};

extern PositionImage merge (std::list<PositionImage> images);
extern bool operator== (Image const & a, Image const & b);

#endif
