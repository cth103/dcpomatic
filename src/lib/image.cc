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

/** @file src/image.cc
 *  @brief A set of classes to describe video images.
 */

#include <sstream>
#include <iomanip>
#include <iostream>
#include <execinfo.h>
#include <cxxabi.h>
#include <sys/time.h>
#include <boost/algorithm/string.hpp>
#include <openjpeg.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libpostproc/postprocess.h>
#include <libavutil/pixfmt.h>
}
#include "image.h"
#include "exceptions.h"
#include "scaler.h"

#ifdef DEBUG_HASH
#include <mhash.h>
#endif

using namespace std;
using namespace boost;

/** @param n Component index.
 *  @return Number of lines in the image for the given component.
 */
int
Image::lines (int n) const
{
	switch (_pixel_format) {
	case PIX_FMT_YUV420P:
		if (n == 0) {
			return size().height;
		} else {
			return size().height / 2;
		}
		break;
	case PIX_FMT_RGB24:
		return size().height;
	default:
		assert (false);
	}

	return 0;
}

/** @return Number of components */
int
Image::components () const
{
	switch (_pixel_format) {
	case PIX_FMT_YUV420P:
		return 3;
	case PIX_FMT_RGB24:
		return 1;
	default:
		assert (false);
	}

	return 0;
}

#ifdef DEBUG_HASH
/** Write a MD5 hash of the image's data to stdout.
 *  @param n Title to give the output.
 */
void
Image::hash (string n) const
{
	MHASH ht = mhash_init (MHASH_MD5);
	if (ht == MHASH_FAILED) {
		throw EncodeError ("could not create hash thread");
	}
	
	for (int i = 0; i < components(); ++i) {
		mhash (ht, data()[i], line_size()[i] * lines(i));
	}
	
	uint8_t hash[16];
	mhash_deinit (ht, hash);
	
	printf ("%s: ", n.c_str ());
	for (int i = 0; i < int (mhash_get_block_size (MHASH_MD5)); ++i) {
		printf ("%.2x", hash[i]);
	}
	printf ("\n");
}
#endif

/** Scale this image to a given size and convert it to RGB.
 *  @param out_size Output image size in pixels.
 *  @param scaler Scaler to use.
 */
shared_ptr<RGBFrameImage>
Image::scale_and_convert_to_rgb (Size out_size, int padding, Scaler const * scaler) const
{
	assert (scaler);

	Size content_size = out_size;
	content_size.width -= (padding * 2);

	shared_ptr<RGBFrameImage> rgb (new RGBFrameImage (content_size));
	
	struct SwsContext* scale_context = sws_getContext (
		size().width, size().height, pixel_format(),
		content_size.width, content_size.height, PIX_FMT_RGB24,
		scaler->ffmpeg_id (), 0, 0, 0
		);

	/* Scale and convert to RGB from whatever its currently in (which may be RGB) */
	sws_scale (
		scale_context,
		data(), line_size(),
		0, size().height,
		rgb->data (), rgb->line_size ()
		);

	/* Put the image in the right place in a black frame if are padding; this is
	   a bit grubby and expensive, but probably inconsequential in the great
	   scheme of things.
	*/
	if (padding > 0) {
		shared_ptr<RGBFrameImage> padded_rgb (new RGBFrameImage (out_size));
		padded_rgb->make_black ();

		/* XXX: we are cheating a bit here; we know the frame is RGB so we can
		   make assumptions about its composition.
		*/
		uint8_t* p = padded_rgb->data()[0] + padding * 3;
		uint8_t* q = rgb->data()[0];
		for (int j = 0; j < rgb->lines(0); ++j) {
			memcpy (p, q, rgb->line_size()[0]);
			p += padded_rgb->line_size()[0];
			q += rgb->line_size()[0];
		}

		rgb = padded_rgb;
	}

	sws_freeContext (scale_context);

	return rgb;
}

/** Run a FFmpeg post-process on this image and return the processed version.
 *  @param pp Flags for the required set of post processes.
 *  @return Post-processed image.
 */
shared_ptr<PostProcessImage>
Image::post_process (string pp) const
{
	shared_ptr<PostProcessImage> out (new PostProcessImage (PIX_FMT_YUV420P, size ()));
	
	pp_mode* mode = pp_get_mode_by_name_and_quality (pp.c_str (), PP_QUALITY_MAX);
	pp_context* context = pp_get_context (size().width, size().height, PP_FORMAT_420 | PP_CPU_CAPS_MMX2);

	pp_postprocess (
		(const uint8_t **) data(), line_size(),
		out->data(), out->line_size(),
		size().width, size().height,
		0, 0, mode, context, 0
		);
		
	pp_free_mode (mode);
	pp_free_context (context);

	return out;
}

void
Image::make_black ()
{
	switch (_pixel_format) {
	case PIX_FMT_YUV420P:
		memset (data()[0], 0, lines(0) * line_size()[0]);
		memset (data()[1], 0x80, lines(1) * line_size()[1]);
		memset (data()[2], 0x80, lines(2) * line_size()[2]);
		break;

	case PIX_FMT_RGB24:		
		memset (data()[0], 0, lines(0) * line_size()[0]);
		break;

	default:
		assert (false);
	}
}

/** Construct a SimpleImage of a given size and format, allocating memory
 *  as required.
 *
 *  @param p Pixel format.
 *  @param s Size in pixels.
 */
SimpleImage::SimpleImage (PixelFormat p, Size s)
	: Image (p)
	, _size (s)
{
	_data = (uint8_t **) av_malloc (components() * sizeof (uint8_t *));
	_line_size = (int *) av_malloc (components() * sizeof (int));
	
	for (int i = 0; i < components(); ++i) {
		_data[i] = 0;
		_line_size[i] = 0;
	}
}

/** Destroy a SimpleImage */
SimpleImage::~SimpleImage ()
{
	for (int i = 0; i < components(); ++i) {
		av_free (_data[i]);
	}

	av_free (_data);
	av_free (_line_size);
}

/** Set the size in bytes of each horizontal line of a given component.
 *  @param i Component index.
 *  @param s Size of line in bytes.
 */
void
SimpleImage::set_line_size (int i, int s)
{
	_line_size[i] = s;
	_data[i] = (uint8_t *) av_malloc (s * lines (i));
}

uint8_t **
SimpleImage::data () const
{
	return _data;
}

int *
SimpleImage::line_size () const
{
	return _line_size;
}

Size
SimpleImage::size () const
{
	return _size;
}


FilterBufferImage::FilterBufferImage (PixelFormat p, AVFilterBufferRef* b)
	: Image (p)
	, _buffer (b)
{

}

FilterBufferImage::~FilterBufferImage ()
{
	avfilter_unref_buffer (_buffer);
}

uint8_t **
FilterBufferImage::data () const
{
	return _buffer->data;
}

int *
FilterBufferImage::line_size () const
{
	return _buffer->linesize;
}

Size
FilterBufferImage::size () const
{
	return Size (_buffer->video->w, _buffer->video->h);
}

/** XXX: this could be generalised to use any format, but I don't
 *  understand how avpicture_fill is supposed to be called with
 *  multi-planar images.
 */
RGBFrameImage::RGBFrameImage (Size s)
	: Image (PIX_FMT_RGB24)
	, _size (s)
{
	_frame = avcodec_alloc_frame ();
	if (_frame == 0) {
		throw EncodeError ("could not allocate frame");
	}

	_data = (uint8_t *) av_malloc (size().width * size().height * 3);
	avpicture_fill ((AVPicture *) _frame, _data, PIX_FMT_RGB24, size().width, size().height);
	_frame->width = size().width;
	_frame->height = size().height;
	_frame->format = PIX_FMT_RGB24;
}

RGBFrameImage::~RGBFrameImage ()
{
	av_free (_data);
	av_free (_frame);
}

uint8_t **
RGBFrameImage::data () const
{
	return _frame->data;
}

int *
RGBFrameImage::line_size () const
{
	return _frame->linesize;
}

Size
RGBFrameImage::size () const
{
	return _size;
}

PostProcessImage::PostProcessImage (PixelFormat p, Size s)
	: Image (p)
	, _size (s)
{
	_data = new uint8_t*[4];
	_line_size = new int[4];
	
	for (int i = 0; i < 4; ++i) {
		_data[i] = (uint8_t *) av_malloc (s.width * s.height);
		_line_size[i] = s.width;
	}
}

PostProcessImage::~PostProcessImage ()
{
	for (int i = 0; i < 4; ++i) {
		av_free (_data[i]);
	}
	
	delete[] _data;
	delete[] _line_size;
}

uint8_t **
PostProcessImage::data () const
{
	return _data;
}

int *
PostProcessImage::line_size () const
{
	return _line_size;
}

Size
PostProcessImage::size () const
{
	return _size;
}
