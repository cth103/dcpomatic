/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
 *  @brief A class to describe a video image.
 */

#include <iostream>
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
}
#include "image.h"
#include "exceptions.h"
#include "scaler.h"
#include "timer.h"
#include "rect.h"
#include "md5_digester.h"

#include "i18n.h"

using std::string;
using std::min;
using std::cout;
using std::cerr;
using std::list;
using boost::shared_ptr;
using dcp::Size;

int
Image::line_factor (int n) const
{
	if (n == 0) {
		return 1;
	}

	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("lines()", _pixel_format);
	}
	
	return pow (2.0f, d->log2_chroma_h);
}

/** @param n Component index.
 *  @return Number of lines in the image for the given component.
 */
int
Image::lines (int n) const
{
	return rint (ceil (static_cast<double>(size().height) / line_factor (n)));
}

/** @return Number of components */
int
Image::components () const
{
	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("components()", _pixel_format);
	}

	if ((d->flags & PIX_FMT_PLANAR) == 0) {
		return 1;
	}
	
	return d->nb_components;
}

/** Crop this image, scale it to `inter_size' and then place it in a black frame of `out_size' */
shared_ptr<Image>
Image::crop_scale_window (Crop crop, dcp::Size inter_size, dcp::Size out_size, Scaler const * scaler, AVPixelFormat out_format, bool out_aligned) const
{
	DCPOMATIC_ASSERT (scaler);
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image is not aligned.
	*/
	DCPOMATIC_ASSERT (aligned ());

	DCPOMATIC_ASSERT (out_size.width >= inter_size.width);
	DCPOMATIC_ASSERT (out_size.height >= inter_size.height);

	/* Here's an image of out_size */
	shared_ptr<Image> out (new Image (out_format, out_size, out_aligned));
	out->make_black ();

	/* Size of the image after any crop */
	dcp::Size const cropped_size = crop.apply (size ());

	/* Scale context for a scale from cropped_size to inter_size */
	struct SwsContext* scale_context = sws_getContext (
			cropped_size.width, cropped_size.height, pixel_format(),
			inter_size.width, inter_size.height, out_format,
			scaler->ffmpeg_id (), 0, 0, 0
		);

	if (!scale_context) {
		throw StringError (N_("Could not allocate SwsContext"));
	}

	/* Prepare input data pointers with crop */
	uint8_t* scale_in_data[components()];
	for (int c = 0; c < components(); ++c) {
		scale_in_data[c] = data()[c] + int (rint (bytes_per_pixel(c) * crop.left)) + stride()[c] * (crop.top / line_factor(c));
	}

	/* Corner of the image within out_size */
	Position<int> const corner ((out_size.width - inter_size.width) / 2, (out_size.height - inter_size.height) / 2);

	uint8_t* scale_out_data[out->components()];
	for (int c = 0; c < out->components(); ++c) {
		scale_out_data[c] = out->data()[c] + int (rint (out->bytes_per_pixel(c) * corner.x)) + out->stride()[c] * corner.y;
	}

	sws_scale (
		scale_context,
		scale_in_data, stride(),
		0, cropped_size.height,
		scale_out_data, out->stride()
		);

	sws_freeContext (scale_context);

	return out;	
}

shared_ptr<Image>
Image::scale (dcp::Size out_size, Scaler const * scaler, AVPixelFormat out_format, bool out_aligned) const
{
	DCPOMATIC_ASSERT (scaler);
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image is not aligned.
	*/
	DCPOMATIC_ASSERT (aligned ());

	shared_ptr<Image> scaled (new Image (out_format, out_size, out_aligned));

	struct SwsContext* scale_context = sws_getContext (
		size().width, size().height, pixel_format(),
		out_size.width, out_size.height, out_format,
		scaler->ffmpeg_id (), 0, 0, 0
		);

	sws_scale (
		scale_context,
		data(), stride(),
		0, size().height,
		scaled->data(), scaled->stride()
		);

	sws_freeContext (scale_context);

	return scaled;
}

shared_ptr<Image>
Image::crop (Crop crop, bool aligned) const
{
	dcp::Size cropped_size = crop.apply (size ());
	shared_ptr<Image> out (new Image (pixel_format(), cropped_size, aligned));

	for (int c = 0; c < components(); ++c) {
		int const crop_left_in_bytes = bytes_per_pixel(c) * crop.left;
		/* bytes_per_pixel() could be a fraction; in this case the stride will be rounded
		   up, and we need to make sure that we copy over the width (up to the stride)
		   rather than short of the width; hence the ceil() here.
		*/
		int const cropped_width_in_bytes = ceil (bytes_per_pixel(c) * cropped_size.width);

		/* Start of the source line, cropped from the top but not the left */
		uint8_t* in_p = data()[c] + (crop.top / out->line_factor(c)) * stride()[c];
		uint8_t* out_p = out->data()[c];

		for (int y = 0; y < out->lines(c); ++y) {
			memcpy (out_p, in_p + crop_left_in_bytes, cropped_width_in_bytes);
			in_p += stride()[c];
			out_p += out->stride()[c];
		}
	}

	return out;
}

/** Blacken a YUV image whose bits per pixel is rounded up to 16 */
void
Image::yuv_16_black (uint16_t v, bool alpha)
{
	memset (data()[0], 0, lines(0) * stride()[0]);
	for (int i = 1; i < 3; ++i) {
		int16_t* p = reinterpret_cast<int16_t*> (data()[i]);
		for (int y = 0; y < lines(i); ++y) {
			/* We divide by 2 here because we are writing 2 bytes at a time */
			for (int x = 0; x < line_size()[i] / 2; ++x) {
				p[x] = v;
			}
			p += stride()[i] / 2;
		}
	}

	if (alpha) {
		memset (data()[3], 0, lines(3) * stride()[3]);
	}
}

uint16_t
Image::swap_16 (uint16_t v)
{
	return ((v >> 8) & 0xff) | ((v & 0xff) << 8);
}

void
Image::make_black ()
{
	/* U/V black value for 8-bit colour */
	static uint8_t const eight_bit_uv =	(1 << 7) - 1;
	/* U/V black value for 9-bit colour */
	static uint16_t const nine_bit_uv =	(1 << 8) - 1;
	/* U/V black value for 10-bit colour */
	static uint16_t const ten_bit_uv =	(1 << 9) - 1;
	/* U/V black value for 16-bit colour */
	static uint16_t const sixteen_bit_uv =	(1 << 15) - 1;
	
	switch (_pixel_format) {
	case PIX_FMT_YUV420P:
	case PIX_FMT_YUV422P:
	case PIX_FMT_YUV444P:
	case PIX_FMT_YUV411P:
		memset (data()[0], 0, lines(0) * stride()[0]);
		memset (data()[1], eight_bit_uv, lines(1) * stride()[1]);
		memset (data()[2], eight_bit_uv, lines(2) * stride()[2]);
		break;

	case PIX_FMT_YUVJ420P:
	case PIX_FMT_YUVJ422P:
	case PIX_FMT_YUVJ444P:
		memset (data()[0], 0, lines(0) * stride()[0]);
		memset (data()[1], eight_bit_uv + 1, lines(1) * stride()[1]);
		memset (data()[2], eight_bit_uv + 1, lines(2) * stride()[2]);
		break;

	case PIX_FMT_YUV422P9LE:
	case PIX_FMT_YUV444P9LE:
		yuv_16_black (nine_bit_uv, false);
		break;

	case PIX_FMT_YUV422P9BE:
	case PIX_FMT_YUV444P9BE:
		yuv_16_black (swap_16 (nine_bit_uv), false);
		break;
		
	case PIX_FMT_YUV422P10LE:
	case PIX_FMT_YUV444P10LE:
		yuv_16_black (ten_bit_uv, false);
		break;

	case PIX_FMT_YUV422P16LE:
	case PIX_FMT_YUV444P16LE:
		yuv_16_black (sixteen_bit_uv, false);
		break;
		
	case PIX_FMT_YUV444P10BE:
	case PIX_FMT_YUV422P10BE:
		yuv_16_black (swap_16 (ten_bit_uv), false);
		break;

	case AV_PIX_FMT_YUVA420P9BE:
	case AV_PIX_FMT_YUVA422P9BE:
	case AV_PIX_FMT_YUVA444P9BE:
		yuv_16_black (swap_16 (nine_bit_uv), true);
		break;
		
	case AV_PIX_FMT_YUVA420P9LE:
	case AV_PIX_FMT_YUVA422P9LE:
	case AV_PIX_FMT_YUVA444P9LE:
		yuv_16_black (nine_bit_uv, true);
		break;
		
	case AV_PIX_FMT_YUVA420P10BE:
	case AV_PIX_FMT_YUVA422P10BE:
	case AV_PIX_FMT_YUVA444P10BE:
		yuv_16_black (swap_16 (ten_bit_uv), true);
		break;
		
	case AV_PIX_FMT_YUVA420P10LE:
	case AV_PIX_FMT_YUVA422P10LE:
	case AV_PIX_FMT_YUVA444P10LE:
		yuv_16_black (ten_bit_uv, true);
		break;
		
	case AV_PIX_FMT_YUVA420P16BE:
	case AV_PIX_FMT_YUVA422P16BE:
	case AV_PIX_FMT_YUVA444P16BE:
		yuv_16_black (swap_16 (sixteen_bit_uv), true);
		break;
		
	case AV_PIX_FMT_YUVA420P16LE:
	case AV_PIX_FMT_YUVA422P16LE:
	case AV_PIX_FMT_YUVA444P16LE:
		yuv_16_black (sixteen_bit_uv, true);
		break;

	case PIX_FMT_RGB24:
	case PIX_FMT_ARGB:
	case PIX_FMT_RGBA:
	case PIX_FMT_ABGR:
	case PIX_FMT_BGRA:
	case PIX_FMT_RGB555LE:
	case PIX_FMT_RGB48LE:
	case PIX_FMT_RGB48BE:
		memset (data()[0], 0, lines(0) * stride()[0]);
		break;

	case PIX_FMT_UYVY422:
	{
		int const Y = lines(0);
		int const X = line_size()[0];
		uint8_t* p = data()[0];
		for (int y = 0; y < Y; ++y) {
			for (int x = 0; x < X / 4; ++x) {
				*p++ = eight_bit_uv; // Cb
				*p++ = 0;	     // Y0
				*p++ = eight_bit_uv; // Cr
				*p++ = 0;	     // Y1
			}
		}
		break;
	}

	default:
		throw PixelFormatError ("make_black()", _pixel_format);
	}
}

void
Image::make_transparent ()
{
	if (_pixel_format != PIX_FMT_RGBA) {
		throw PixelFormatError ("make_transparent()", _pixel_format);
	}

	memset (data()[0], 0, lines(0) * stride()[0]);
}

void
Image::alpha_blend (shared_ptr<const Image> other, Position<int> position)
{
	DCPOMATIC_ASSERT (other->pixel_format() == PIX_FMT_RGBA);
	int const other_bpp = 4;

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

	switch (_pixel_format) {
	case PIX_FMT_RGB24:
	{
		int const this_bpp = 3;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				tp[0] = op[0] + (tp[0] * (1 - alpha));
				tp[1] = op[1] + (tp[1] * (1 - alpha));
				tp[2] = op[2] + (tp[2] * (1 - alpha));
				
				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case PIX_FMT_BGRA:
	case PIX_FMT_RGBA:
	{
		int const this_bpp = 4;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				tp[0] = op[0] + (tp[0] * (1 - alpha));
				tp[1] = op[1] + (tp[1] * (1 - alpha));
				tp[2] = op[2] + (tp[2] * (1 - alpha));
				tp[3] = op[3] + (tp[3] * (1 - alpha));
				
				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case PIX_FMT_RGB48LE:
	{
		int const this_bpp = 6;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				/* Blend high bytes */
				tp[1] = op[0] + (tp[1] * (1 - alpha));
				tp[3] = op[1] + (tp[3] * (1 - alpha));
				tp[5] = op[2] + (tp[5] * (1 - alpha));
				
				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	default:
		DCPOMATIC_ASSERT (false);
	}
}
	
void
Image::copy (shared_ptr<const Image> other, Position<int> position)
{
	/* Only implemented for RGB24 onto RGB24 so far */
	DCPOMATIC_ASSERT (_pixel_format == PIX_FMT_RGB24 && other->pixel_format() == PIX_FMT_RGB24);
	DCPOMATIC_ASSERT (position.x >= 0 && position.y >= 0);

	int const N = min (position.x + other->size().width, size().width) - position.x;
	for (int ty = position.y, oy = 0; ty < size().height && oy < other->size().height; ++ty, ++oy) {
		uint8_t * const tp = data()[0] + ty * stride()[0] + position.x * 3;
		uint8_t * const op = other->data()[0] + oy * other->stride()[0];
		memcpy (tp, op, N * 3);
	}
}	

void
Image::read_from_socket (shared_ptr<Socket> socket)
{
	for (int i = 0; i < components(); ++i) {
		uint8_t* p = data()[i];
		for (int y = 0; y < lines(i); ++y) {
			socket->read (p, line_size()[i]);
			p += stride()[i];
		}
	}
}

void
Image::write_to_socket (shared_ptr<Socket> socket) const
{
	for (int i = 0; i < components(); ++i) {
		uint8_t* p = data()[i];
		for (int y = 0; y < lines(i); ++y) {
			socket->write (p, line_size()[i]);
			p += stride()[i];
		}
	}
}


float
Image::bytes_per_pixel (int c) const
{
	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("lines()", _pixel_format);
	}

	if (c >= components()) {
		return 0;
	}

	float bpp[4] = { 0, 0, 0, 0 };

	bpp[0] = floor ((d->comp[0].depth_minus1 + 1 + 7) / 8);
	if (d->nb_components > 1) {
		bpp[1] = floor ((d->comp[1].depth_minus1 + 1 + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 2) {
		bpp[2] = floor ((d->comp[2].depth_minus1 + 1 + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 3) {
		bpp[3] = floor ((d->comp[3].depth_minus1 + 1 + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	
	if ((d->flags & PIX_FMT_PLANAR) == 0) {
		/* Not planar; sum them up */
		return bpp[0] + bpp[1] + bpp[2] + bpp[3];
	}

	return bpp[c];
}

/** Construct a Image of a given size and format, allocating memory
 *  as required.
 *
 *  @param p Pixel format.
 *  @param s Size in pixels.
 */
Image::Image (AVPixelFormat p, dcp::Size s, bool aligned)
	: dcp::Image (s)
	, _pixel_format (p)
	, _aligned (aligned)
{
	allocate ();
}

void
Image::allocate ()
{
	_data = (uint8_t **) wrapped_av_malloc (4 * sizeof (uint8_t *));
	_data[0] = _data[1] = _data[2] = _data[3] = 0;
	
	_line_size = (int *) wrapped_av_malloc (4 * sizeof (int));
	_line_size[0] = _line_size[1] = _line_size[2] = _line_size[3] = 0;
	
	_stride = (int *) wrapped_av_malloc (4 * sizeof (int));
	_stride[0] = _stride[1] = _stride[2] = _stride[3] = 0;

	for (int i = 0; i < components(); ++i) {
		_line_size[i] = ceil (_size.width * bytes_per_pixel(i));
		_stride[i] = stride_round_up (i, _line_size, _aligned ? 32 : 1);

		/* The assembler function ff_rgb24ToY_avx (in libswscale/x86/input.asm)
		   uses a 16-byte fetch to read three bytes (R/G/B) of image data.
		   Hence on the last pixel of the last line it reads over the end of
		   the actual data by 1 byte.  If the width of an image is a multiple
		   of the stride alignment there will be no padding at the end of image lines.
		   OS X crashes on this illegal read, though other operating systems don't
		   seem to mind.  The nasty + 1 in this malloc makes sure there is always a byte
		   for that instruction to read safely.

		   Further to the above, valgrind is now telling me that ff_rgb24ToY_ssse3
		   over-reads by more then _avx.  I can't follow the code to work out how much,
		   so I'll just over-allocate by 32 bytes and have done with it.  Empirical
		   testing suggests that it works.
		*/
		_data[i] = (uint8_t *) wrapped_av_malloc (_stride[i] * lines (i) + 32);
	}
}

Image::Image (Image const & other)
	: dcp::Image (other)
	,  _pixel_format (other._pixel_format)
	, _aligned (other._aligned)
{
	allocate ();

	for (int i = 0; i < components(); ++i) {
		uint8_t* p = _data[i];
		uint8_t* q = other._data[i];
		for (int j = 0; j < lines(i); ++j) {
			memcpy (p, q, _line_size[i]);
			p += stride()[i];
			q += other.stride()[i];
		}
	}
}

Image::Image (AVFrame* frame)
	: dcp::Image (dcp::Size (frame->width, frame->height))
	, _pixel_format (static_cast<AVPixelFormat> (frame->format))
	, _aligned (true)
{
	allocate ();

	for (int i = 0; i < components(); ++i) {
		uint8_t* p = _data[i];
		uint8_t* q = frame->data[i];
		for (int j = 0; j < lines(i); ++j) {
			memcpy (p, q, _line_size[i]);
			p += stride()[i];
			/* AVFrame's linesize is what we call `stride' */
			q += frame->linesize[i];
		}
	}
}

Image::Image (shared_ptr<const Image> other, bool aligned)
	: dcp::Image (other)
	, _pixel_format (other->_pixel_format)
	, _aligned (aligned)
{
	allocate ();

	for (int i = 0; i < components(); ++i) {
		DCPOMATIC_ASSERT (line_size()[i] == other->line_size()[i]);
		uint8_t* p = _data[i];
		uint8_t* q = other->data()[i];
		for (int j = 0; j < lines(i); ++j) {
			memcpy (p, q, line_size()[i]);
			p += stride()[i];
			q += other->stride()[i];
		}
	}
}

Image&
Image::operator= (Image const & other)
{
	if (this == &other) {
		return *this;
	}

	Image tmp (other);
	swap (tmp);
	return *this;
}

void
Image::swap (Image & other)
{
	dcp::Image::swap (other);
	
	std::swap (_pixel_format, other._pixel_format);

	for (int i = 0; i < 4; ++i) {
		std::swap (_data[i], other._data[i]);
		std::swap (_line_size[i], other._line_size[i]);
		std::swap (_stride[i], other._stride[i]);
	}

	std::swap (_aligned, other._aligned);
}

/** Destroy a Image */
Image::~Image ()
{
	for (int i = 0; i < components(); ++i) {
		av_free (_data[i]);
	}

	av_free (_data);
	av_free (_line_size);
	av_free (_stride);
}

uint8_t **
Image::data () const
{
	return _data;
}

int *
Image::line_size () const
{
	return _line_size;
}

int *
Image::stride () const
{
	return _stride;
}

dcp::Size
Image::size () const
{
	return _size;
}

bool
Image::aligned () const
{
	return _aligned;
}

PositionImage
merge (list<PositionImage> images)
{
	if (images.empty ()) {
		return PositionImage ();
	}

	if (images.size() == 1) {
		return images.front ();
	}

	dcpomatic::Rect<int> all (images.front().position, images.front().image->size().width, images.front().image->size().height);
	for (list<PositionImage>::const_iterator i = images.begin(); i != images.end(); ++i) {
		all.extend (dcpomatic::Rect<int> (i->position, i->image->size().width, i->image->size().height));
	}

	shared_ptr<Image> merged (new Image (images.front().image->pixel_format (), dcp::Size (all.width, all.height), true));
	merged->make_transparent ();
	for (list<PositionImage>::const_iterator i = images.begin(); i != images.end(); ++i) {
		merged->alpha_blend (i->image, i->position - all.position());
	}

	return PositionImage (merged, all.position ());
}

string
Image::digest () const
{
	MD5Digester digester;

	for (int i = 0; i < components(); ++i) {
		digester.add (data()[i], line_size()[i]);
	}

	return digester.get ();
}

bool
operator== (Image const & a, Image const & b)
{
	if (a.components() != b.components() || a.pixel_format() != b.pixel_format() || a.aligned() != b.aligned()) {
		return false;
	}

	for (int c = 0; c < a.components(); ++c) {
		if (a.lines(c) != b.lines(c) || a.line_size()[c] != b.line_size()[c] || a.stride()[c] != b.stride()[c]) {
			return false;
		}

		uint8_t* p = a.data()[c];
		uint8_t* q = b.data()[c];
		for (int y = 0; y < a.lines(c); ++y) {
			if (memcmp (p, q, a.line_size()[c]) != 0) {
				return false;
			}

			p += a.stride()[c];
			q += b.stride()[c];
		}
	}

	return true;
}

void
Image::fade (float f)
{
	switch (_pixel_format) {
	case PIX_FMT_YUV420P:
	case PIX_FMT_YUV422P:
	case PIX_FMT_YUV444P:
	case PIX_FMT_YUV411P:
	case PIX_FMT_YUVJ420P:
	case PIX_FMT_YUVJ422P:
	case PIX_FMT_YUVJ444P:
	case PIX_FMT_RGB24:
	case PIX_FMT_ARGB:
	case PIX_FMT_RGBA:
	case PIX_FMT_ABGR:
	case PIX_FMT_BGRA:
	case PIX_FMT_RGB555LE:
		/* 8-bit */
		for (int c = 0; c < 3; ++c) {
			uint8_t* p = data()[c];
			for (int y = 0; y < lines(c); ++y) {
				uint8_t* q = p;
				for (int x = 0; x < line_size()[c]; ++x) {
					*q = int (float (*q) * f);
					++q;
				}
				p += stride()[c];
			}
		}
		break;

	case PIX_FMT_YUV422P9LE:
	case PIX_FMT_YUV444P9LE:
	case PIX_FMT_YUV422P10LE:
	case PIX_FMT_YUV444P10LE:
	case PIX_FMT_YUV422P16LE:
	case PIX_FMT_YUV444P16LE:
	case AV_PIX_FMT_YUVA420P9LE:
	case AV_PIX_FMT_YUVA422P9LE:
	case AV_PIX_FMT_YUVA444P9LE:
	case AV_PIX_FMT_YUVA420P10LE:
	case AV_PIX_FMT_YUVA422P10LE:
	case AV_PIX_FMT_YUVA444P10LE:
		/* 16-bit little-endian */
		for (int c = 0; c < 3; ++c) {
			int const stride_pixels = stride()[c] / 2;
			int const line_size_pixels = line_size()[c] / 2;
			uint16_t* p = reinterpret_cast<uint16_t*> (data()[c]);
			for (int y = 0; y < lines(c); ++y) {
				uint16_t* q = p;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = int (float (*q) * f);
					++q;
				}
				p += stride_pixels;
			}
		}
		break;

	case PIX_FMT_YUV422P9BE:
	case PIX_FMT_YUV444P9BE:
	case PIX_FMT_YUV444P10BE:
	case PIX_FMT_YUV422P10BE:
	case AV_PIX_FMT_YUVA420P9BE:
	case AV_PIX_FMT_YUVA422P9BE:
	case AV_PIX_FMT_YUVA444P9BE:
	case AV_PIX_FMT_YUVA420P10BE:
	case AV_PIX_FMT_YUVA422P10BE:
	case AV_PIX_FMT_YUVA444P10BE:
	case AV_PIX_FMT_YUVA420P16BE:
	case AV_PIX_FMT_YUVA422P16BE:
	case AV_PIX_FMT_YUVA444P16BE:
		/* 16-bit big-endian */
		for (int c = 0; c < 3; ++c) {
			int const stride_pixels = stride()[c] / 2;
			int const line_size_pixels = line_size()[c] / 2;
			uint16_t* p = reinterpret_cast<uint16_t*> (data()[c]);
			for (int y = 0; y < lines(c); ++y) {
				uint16_t* q = p;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = swap_16 (int (float (swap_16 (*q)) * f));
					++q;
				}
				p += stride_pixels;
			}
		}
		break;

	case PIX_FMT_UYVY422:
	{
		int const Y = lines(0);
		int const X = line_size()[0];
		uint8_t* p = data()[0];
		for (int y = 0; y < Y; ++y) {
			for (int x = 0; x < X; ++x) {
				*p = int (float (*p) * f);
				++p;
			}
		}
		break;
	}

	default:
		throw PixelFormatError ("fade()", _pixel_format);
	}
}
