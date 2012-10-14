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
	case PIX_FMT_RGBA:
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
	case PIX_FMT_RGBA:
		return 1;
	default:
		assert (false);
	}

	return 0;
}

shared_ptr<Image>
Image::scale (Size out_size, Scaler const * scaler) const
{
	assert (scaler);

	shared_ptr<Image> scaled (new SimpleImage (pixel_format(), out_size));

	struct SwsContext* scale_context = sws_getContext (
		size().width, size().height, pixel_format(),
		out_size.width, out_size.height, pixel_format(),
		scaler->ffmpeg_id (), 0, 0, 0
		);

	sws_scale (
		scale_context,
		data(), line_size(),
		0, size().height,
		scaled->data (), scaled->line_size ()
		);

	sws_freeContext (scale_context);

	return scaled;
}

/** Scale this image to a given size and convert it to RGB.
 *  @param out_size Output image size in pixels.
 *  @param scaler Scaler to use.
 */
shared_ptr<Image>
Image::scale_and_convert_to_rgb (Size out_size, int padding, Scaler const * scaler) const
{
	assert (scaler);

	Size content_size = out_size;
	content_size.width -= (padding * 2);

	shared_ptr<Image> rgb (new SimpleImage (PIX_FMT_RGB24, content_size));

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
		shared_ptr<Image> padded_rgb (new SimpleImage (PIX_FMT_RGB24, out_size));
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
shared_ptr<Image>
Image::post_process (string pp) const
{
	shared_ptr<Image> out (new SimpleImage (PIX_FMT_YUV420P, size ()));
	
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

void
Image::alpha_blend (shared_ptr<Image> other, Position position)
{
	/* Only implemented for RGBA onto RGB24 so far */
	assert (_pixel_format == PIX_FMT_RGB24 && other->pixel_format() == PIX_FMT_RGBA);

	int start_tx = position.x;
	int start_ox = 0;

	if (start_tx < 0) {
		start_ox = -start_tx;
		start_tx = 0;
	}

	int start_ty = position.y;
	int start_oy = 0;

	if (start_ty < 0) {
		start_oy = -start_ty;
		start_ty = 0;
	}

	for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
		uint8_t* tp = data()[0] + ty * line_size()[0] + position.x * 3;
		uint8_t* op = other->data()[0] + oy * other->line_size()[0];
		for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
			float const alpha = float (op[3]) / 255;
			tp[0] = (tp[0] * (1 - alpha)) + op[0] * alpha;
			tp[1] = (tp[1] * (1 - alpha)) + op[1] * alpha;
			tp[2] = (tp[2] * (1 - alpha)) + op[2] * alpha;
			tp += 3;
			op += 4;
		}
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
	_data = (uint8_t **) av_malloc (4 * sizeof (uint8_t *));
	_data[0] = _data[1] = _data[2] = _data[3] = 0;
	_line_size = (int *) av_malloc (4);
	_line_size[0] = _line_size[1] = _line_size[2] = _line_size[3] = 0;

	switch (p) {
	case PIX_FMT_RGB24:
		_line_size[0] = round_up (s.width * 3, 32);
		break;
	case PIX_FMT_RGBA:
		_line_size[0] = round_up (s.width * 4, 32);
		break;
	case PIX_FMT_YUV420P:
		_line_size[0] = round_up (s.width, 32);
		_line_size[1] = round_up (s.width / 2, 32);
		_line_size[2] = round_up (s.width / 2, 32);
		break;
	default:
		assert (false);
	}
	
	for (int i = 0; i < components(); ++i) {
		_data[i] = (uint8_t *) av_malloc (_line_size[i] * lines (i));
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
