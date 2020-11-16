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

/** @file src/image.cc
 *  @brief A class to describe a video image.
 */

#include "image.h"
#include "exceptions.h"
#include "timer.h"
#include "rect.h"
#include "util.h"
#include "compose.hpp"
#include "dcpomatic_socket.h"
#include <dcp/rgb_xyz.h>
#include <dcp/transfer_function.h>
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/frame.h>
}
#include <png.h>
#if HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif
#include <iostream>

#include "i18n.h"

using std::string;
using std::min;
using std::max;
using std::cout;
using std::cerr;
using std::list;
using std::runtime_error;
using boost::shared_ptr;
using dcp::Size;


/** The memory alignment, in bytes, used for each row of an image if aligment is requested */
#define ALIGNMENT 64


int
Image::vertical_factor (int n) const
{
	if (n == 0) {
		return 1;
	}

	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("line_factor()", _pixel_format);
	}

	return lrintf(powf(2.0f, d->log2_chroma_h));
}

int
Image::horizontal_factor (int n) const
{
	if (n == 0) {
		return 1;
	}

	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("sample_size()", _pixel_format);
	}

	return lrintf(powf(2.0f, d->log2_chroma_w));
}

/** @param n Component index.
 *  @return Number of samples (i.e. pixels, unless sub-sampled) in each direction for this component.
 */
dcp::Size
Image::sample_size (int n) const
{
	return dcp::Size (
		lrint (ceil (static_cast<double>(size().width) / horizontal_factor (n))),
		lrint (ceil (static_cast<double>(size().height) / vertical_factor (n)))
		);
}

/** @return Number of planes */
int
Image::planes () const
{
	AVPixFmtDescriptor const * d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("planes()", _pixel_format);
	}

	if (_pixel_format == AV_PIX_FMT_PAL8) {
		return 2;
	}

	if ((d->flags & AV_PIX_FMT_FLAG_PLANAR) == 0) {
		return 1;
	}

	return d->nb_components;
}


static
int
round_width_for_subsampling (int p, AVPixFmtDescriptor const * desc)
{
	return p & ~ ((1 << desc->log2_chroma_w) - 1);
}


static
int
round_height_for_subsampling (int p, AVPixFmtDescriptor const * desc)
{
	return p & ~ ((1 << desc->log2_chroma_h) - 1);
}


/** Crop this image, scale it to `inter_size' and then place it in a black frame of `out_size'.
 *  @param crop Amount to crop by.
 *  @param inter_size Size to scale the cropped image to.
 *  @param out_size Size of output frame; if this is larger than inter_size there will be black padding.
 *  @param yuv_to_rgb YUV to RGB transformation to use, if required.
 *  @param video_range Video range of the image.
 *  @param out_format Output pixel format.
 *  @param out_aligned true to make the output image aligned.
 *  @param out_video_range Video range to use for the output image.
 *  @param fast Try to be fast at the possible expense of quality; at present this means using
 *  fast bilinear rather than bicubic scaling.
 */
shared_ptr<Image>
Image::crop_scale_window (
	Crop crop,
	dcp::Size inter_size,
	dcp::Size out_size,
	dcp::YUVToRGB yuv_to_rgb,
	VideoRange video_range,
	AVPixelFormat out_format,
	VideoRange out_video_range,
	bool out_aligned,
	bool fast
	) const
{
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image is not aligned.
	*/
	DCPOMATIC_ASSERT (aligned ());

	DCPOMATIC_ASSERT (out_size.width >= inter_size.width);
	DCPOMATIC_ASSERT (out_size.height >= inter_size.height);

	shared_ptr<Image> out (new Image(out_format, out_size, out_aligned));
	out->make_black ();

	AVPixFmtDescriptor const * in_desc = av_pix_fmt_desc_get (_pixel_format);
	if (!in_desc) {
		throw PixelFormatError ("crop_scale_window()", _pixel_format);
	}

	/* Round down so that we crop only the number of pixels that is straightforward
	 * considering any subsampling.
	 */
	Crop rounded_crop(
		round_width_for_subsampling(crop.left, in_desc),
		round_width_for_subsampling(crop.right, in_desc),
		round_height_for_subsampling(crop.top, in_desc),
		round_height_for_subsampling(crop.bottom, in_desc)
		);

	/* Size of the image after any crop */
	dcp::Size const cropped_size = rounded_crop.apply (size());

	/* Scale context for a scale from cropped_size to inter_size */
	struct SwsContext* scale_context = sws_getContext (
			cropped_size.width, cropped_size.height, pixel_format(),
			inter_size.width, inter_size.height, out_format,
			fast ? SWS_FAST_BILINEAR : SWS_BICUBIC, 0, 0, 0
		);

	if (!scale_context) {
		throw runtime_error (N_("Could not allocate SwsContext"));
	}

	DCPOMATIC_ASSERT (yuv_to_rgb < dcp::YUV_TO_RGB_COUNT);
	int const lut[dcp::YUV_TO_RGB_COUNT] = {
		SWS_CS_ITU601,
		SWS_CS_ITU709
	};

	/* The 3rd parameter here is:
	   0 -> source range MPEG (i.e. "video", 16-235)
	   1 -> source range JPEG (i.e. "full", 0-255)
	   And the 5th:
	   0 -> destination range MPEG (i.e. "video", 16-235)
	   1 -> destination range JPEG (i.e. "full", 0-255)

	   But remember: sws_setColorspaceDetails ignores these
	   parameters unless the both source and destination images
	   are isYUV or isGray.  (If either is not, it uses video range).
	*/
	sws_setColorspaceDetails (
		scale_context,
		sws_getCoefficients (lut[yuv_to_rgb]), video_range == VIDEO_RANGE_VIDEO ? 0 : 1,
		sws_getCoefficients (lut[yuv_to_rgb]), out_video_range == VIDEO_RANGE_VIDEO ? 0 : 1,
		0, 1 << 16, 1 << 16
		);

	/* Prepare input data pointers with crop */
	uint8_t* scale_in_data[planes()];
	for (int c = 0; c < planes(); ++c) {
		int const x = lrintf(bytes_per_pixel(c) * rounded_crop.left);
		scale_in_data[c] = data()[c] + x + stride()[c] * (rounded_crop.top / vertical_factor(c));
	}

	AVPixFmtDescriptor const * out_desc = av_pix_fmt_desc_get (out_format);
	if (!out_desc) {
		throw PixelFormatError ("crop_scale_window()", out_format);
	}

	/* Corner of the image within out_size */
	Position<int> const corner (
		round_width_for_subsampling((out_size.width - inter_size.width) / 2, out_desc),
		round_height_for_subsampling((out_size.height - inter_size.height) / 2, out_desc)
		);

	uint8_t* scale_out_data[out->planes()];
	for (int c = 0; c < out->planes(); ++c) {
		int const x = lrintf(out->bytes_per_pixel(c) * corner.x);
		scale_out_data[c] = out->data()[c] + x + out->stride()[c] * (corner.y / out->vertical_factor(c));
	}

	sws_scale (
		scale_context,
		scale_in_data, stride(),
		0, cropped_size.height,
		scale_out_data, out->stride()
		);

	sws_freeContext (scale_context);

	if (rounded_crop != Crop() && cropped_size == inter_size) {
		/* We are cropping without any scaling or pixel format conversion, so FFmpeg may have left some
		   data behind in our image.  Clear it out.  It may get to the point where we should just stop
		   trying to be clever with cropping.
		*/
		out->make_part_black (corner.x + cropped_size.width, out_size.width - cropped_size.width);
	}

	return out;
}

shared_ptr<Image>
Image::convert_pixel_format (dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, bool out_aligned, bool fast) const
{
	return scale(size(), yuv_to_rgb, out_format, out_aligned, fast);
}

/** @param out_size Size to scale to.
 *  @param yuv_to_rgb YUVToRGB transform transform to use, if required.
 *  @param out_format Output pixel format.
 *  @param out_aligned true to make an aligned output image.
 *  @param fast Try to be fast at the possible expense of quality; at present this means using
 *  fast bilinear rather than bicubic scaling.
 */
shared_ptr<Image>
Image::scale (dcp::Size out_size, dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, bool out_aligned, bool fast) const
{
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image is not aligned.
	*/
	DCPOMATIC_ASSERT (aligned ());

	shared_ptr<Image> scaled (new Image (out_format, out_size, out_aligned));

	struct SwsContext* scale_context = sws_getContext (
		size().width, size().height, pixel_format(),
		out_size.width, out_size.height, out_format,
		(fast ? SWS_FAST_BILINEAR : SWS_BICUBIC) | SWS_ACCURATE_RND, 0, 0, 0
		);

	DCPOMATIC_ASSERT (yuv_to_rgb < dcp::YUV_TO_RGB_COUNT);
	int const lut[dcp::YUV_TO_RGB_COUNT] = {
		SWS_CS_ITU601,
		SWS_CS_ITU709
	};

	/* The 3rd parameter here is:
	   0 -> source range MPEG (i.e. "video", 16-235)
	   1 -> source range JPEG (i.e. "full", 0-255)
	   And the 5th:
	   0 -> destination range MPEG (i.e. "video", 16-235)
	   1 -> destination range JPEG (i.e. "full", 0-255)

	   But remember: sws_setColorspaceDetails ignores these
	   parameters unless the corresponding image isYUV or isGray.
	   (If it's neither, it uses video range).
	*/
	sws_setColorspaceDetails (
		scale_context,
		sws_getCoefficients (lut[yuv_to_rgb]), 0,
		sws_getCoefficients (lut[yuv_to_rgb]), 0,
		0, 1 << 16, 1 << 16
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

/** Blacken a YUV image whose bits per pixel is rounded up to 16 */
void
Image::yuv_16_black (uint16_t v, bool alpha)
{
	memset (data()[0], 0, sample_size(0).height * stride()[0]);
	for (int i = 1; i < 3; ++i) {
		int16_t* p = reinterpret_cast<int16_t*> (data()[i]);
		int const lines = sample_size(i).height;
		for (int y = 0; y < lines; ++y) {
			/* We divide by 2 here because we are writing 2 bytes at a time */
			for (int x = 0; x < line_size()[i] / 2; ++x) {
				p[x] = v;
			}
			p += stride()[i] / 2;
		}
	}

	if (alpha) {
		memset (data()[3], 0, sample_size(3).height * stride()[3]);
	}
}

uint16_t
Image::swap_16 (uint16_t v)
{
	return ((v >> 8) & 0xff) | ((v & 0xff) << 8);
}

void
Image::make_part_black (int x, int w)
{
	switch (_pixel_format) {
	case AV_PIX_FMT_RGB24:
	case AV_PIX_FMT_ARGB:
	case AV_PIX_FMT_RGBA:
	case AV_PIX_FMT_ABGR:
	case AV_PIX_FMT_BGRA:
	case AV_PIX_FMT_RGB555LE:
	case AV_PIX_FMT_RGB48LE:
	case AV_PIX_FMT_RGB48BE:
	case AV_PIX_FMT_XYZ12LE:
	{
		int const h = sample_size(0).height;
		int const bpp = bytes_per_pixel(0);
		int const s = stride()[0];
		uint8_t* p = data()[0];
		for (int y = 0; y < h; y++) {
			memset (p + x * bpp, 0, w * bpp);
			p += s;
		}
		break;
	}

	default:
		throw PixelFormatError ("make_part_black()", _pixel_format);
	}
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
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUV444P:
	case AV_PIX_FMT_YUV411P:
		memset (data()[0], 0, sample_size(0).height * stride()[0]);
		memset (data()[1], eight_bit_uv, sample_size(1).height * stride()[1]);
		memset (data()[2], eight_bit_uv, sample_size(2).height * stride()[2]);
		break;

	case AV_PIX_FMT_YUVJ420P:
	case AV_PIX_FMT_YUVJ422P:
	case AV_PIX_FMT_YUVJ444P:
		memset (data()[0], 0, sample_size(0).height * stride()[0]);
		memset (data()[1], eight_bit_uv + 1, sample_size(1).height * stride()[1]);
		memset (data()[2], eight_bit_uv + 1, sample_size(2).height * stride()[2]);
		break;

	case AV_PIX_FMT_YUV422P9LE:
	case AV_PIX_FMT_YUV444P9LE:
		yuv_16_black (nine_bit_uv, false);
		break;

	case AV_PIX_FMT_YUV422P9BE:
	case AV_PIX_FMT_YUV444P9BE:
		yuv_16_black (swap_16 (nine_bit_uv), false);
		break;

	case AV_PIX_FMT_YUV422P10LE:
	case AV_PIX_FMT_YUV444P10LE:
		yuv_16_black (ten_bit_uv, false);
		break;

	case AV_PIX_FMT_YUV422P16LE:
	case AV_PIX_FMT_YUV444P16LE:
		yuv_16_black (sixteen_bit_uv, false);
		break;

	case AV_PIX_FMT_YUV444P10BE:
	case AV_PIX_FMT_YUV422P10BE:
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

	case AV_PIX_FMT_RGB24:
	case AV_PIX_FMT_ARGB:
	case AV_PIX_FMT_RGBA:
	case AV_PIX_FMT_ABGR:
	case AV_PIX_FMT_BGRA:
	case AV_PIX_FMT_RGB555LE:
	case AV_PIX_FMT_RGB48LE:
	case AV_PIX_FMT_RGB48BE:
	case AV_PIX_FMT_XYZ12LE:
		memset (data()[0], 0, sample_size(0).height * stride()[0]);
		break;

	case AV_PIX_FMT_UYVY422:
	{
		int const Y = sample_size(0).height;
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
	if (_pixel_format != AV_PIX_FMT_BGRA && _pixel_format != AV_PIX_FMT_RGBA) {
		throw PixelFormatError ("make_transparent()", _pixel_format);
	}

	memset (data()[0], 0, sample_size(0).height * stride()[0]);
}

void
Image::alpha_blend (shared_ptr<const Image> other, Position<int> position)
{
	/* We're blending RGBA or BGRA images */
	DCPOMATIC_ASSERT (other->pixel_format() == AV_PIX_FMT_BGRA || other->pixel_format() == AV_PIX_FMT_RGBA);
	int const blue = other->pixel_format() == AV_PIX_FMT_BGRA ? 0 : 2;
	int const red = other->pixel_format() == AV_PIX_FMT_BGRA ? 2 : 0;

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
	case AV_PIX_FMT_RGB24:
	{
		/* Going onto RGB24.  First byte is red, second green, third blue */
		int const this_bpp = 3;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				tp[0] = op[red] * alpha + tp[0] * (1 - alpha);
				tp[1] = op[1] * alpha + tp[1] * (1 - alpha);
				tp[2] = op[blue] * alpha + tp[2] * (1 - alpha);

				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case AV_PIX_FMT_BGRA:
	{
		int const this_bpp = 4;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				tp[0] = op[blue] * alpha + tp[0] * (1 - alpha);
				tp[1] = op[1] * alpha + tp[1] * (1 - alpha);
				tp[2] = op[red] * alpha + tp[2] * (1 - alpha);
				tp[3] = op[3] * alpha + tp[3] * (1 - alpha);

				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case AV_PIX_FMT_RGBA:
	{
		int const this_bpp = 4;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				tp[0] = op[red] * alpha + tp[0] * (1 - alpha);
				tp[1] = op[1] * alpha + tp[1] * (1 - alpha);
				tp[2] = op[blue] * alpha + tp[2] * (1 - alpha);
				tp[3] = op[3] * alpha + tp[3] * (1 - alpha);

				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case AV_PIX_FMT_RGB48LE:
	{
		int const this_bpp = 6;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint8_t* tp = data()[0] + ty * stride()[0] + start_tx * this_bpp;
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;
				/* Blend high bytes */
				tp[1] = op[red] * alpha + tp[1] * (1 - alpha);
				tp[3] = op[1] * alpha + tp[3] * (1 - alpha);
				tp[5] = op[blue] * alpha + tp[5] * (1 - alpha);

				tp += this_bpp;
				op += other_bpp;
			}
		}
		break;
	}
	case AV_PIX_FMT_XYZ12LE:
	{
		dcp::ColourConversion conv = dcp::ColourConversion::srgb_to_xyz();
		double fast_matrix[9];
		dcp::combined_rgb_to_xyz (conv, fast_matrix);
		double const * lut_in = conv.in()->lut (8, false);
		double const * lut_out = conv.out()->lut (16, true);
		int const this_bpp = 6;
		for (int ty = start_ty, oy = start_oy; ty < size().height && oy < other->size().height; ++ty, ++oy) {
			uint16_t* tp = reinterpret_cast<uint16_t*> (data()[0] + ty * stride()[0] + start_tx * this_bpp);
			uint8_t* op = other->data()[0] + oy * other->stride()[0];
			for (int tx = start_tx, ox = start_ox; tx < size().width && ox < other->size().width; ++tx, ++ox) {
				float const alpha = float (op[3]) / 255;

				/* Convert sRGB to XYZ; op is BGRA.  First, input gamma LUT */
				double const r = lut_in[op[red]];
				double const g = lut_in[op[1]];
				double const b = lut_in[op[blue]];

				/* RGB to XYZ, including Bradford transform and DCI companding */
				double const x = max (0.0, min (65535.0, r * fast_matrix[0] + g * fast_matrix[1] + b * fast_matrix[2]));
				double const y = max (0.0, min (65535.0, r * fast_matrix[3] + g * fast_matrix[4] + b * fast_matrix[5]));
				double const z = max (0.0, min (65535.0, r * fast_matrix[6] + g * fast_matrix[7] + b * fast_matrix[8]));

				/* Out gamma LUT and blend */
				tp[0] = lrint(lut_out[lrint(x)] * 65535) * alpha + tp[0] * (1 - alpha);
				tp[1] = lrint(lut_out[lrint(y)] * 65535) * alpha + tp[1] * (1 - alpha);
				tp[2] = lrint(lut_out[lrint(z)] * 65535) * alpha + tp[2] * (1 - alpha);

				tp += this_bpp / 2;
				op += other_bpp;
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV420P:
	{
		shared_ptr<Image> yuv = other->convert_pixel_format (dcp::YUV_TO_RGB_REC709, _pixel_format, false, false);
		dcp::Size const ts = size();
		dcp::Size const os = yuv->size();
		for (int ty = start_ty, oy = start_oy; ty < ts.height && oy < os.height; ++ty, ++oy) {
			int const hty = ty / 2;
			int const hoy = oy / 2;
			uint8_t* tY = data()[0] + (ty * stride()[0]) + start_tx;
			uint8_t* tU = data()[1] + (hty * stride()[1]) + start_tx / 2;
			uint8_t* tV = data()[2] + (hty * stride()[2]) + start_tx / 2;
			uint8_t* oY = yuv->data()[0] + (oy * yuv->stride()[0]) + start_ox;
			uint8_t* oU = yuv->data()[1] + (hoy * yuv->stride()[1]) + start_ox / 2;
			uint8_t* oV = yuv->data()[2] + (hoy * yuv->stride()[2]) + start_ox / 2;
			uint8_t* alpha = other->data()[0] + (oy * other->stride()[0]) + start_ox * 4;
			for (int tx = start_tx, ox = start_ox; tx < ts.width && ox < os.width; ++tx, ++ox) {
				float const a = float(alpha[3]) / 255;
				*tY = *oY * a + *tY * (1 - a);
				*tU = *oU * a + *tU * (1 - a);
				*tV = *oV * a + *tV * (1 - a);
				++tY;
				++oY;
				if (tx % 2) {
					++tU;
					++tV;
				}
				if (ox % 2) {
					++oU;
					++oV;
				}
				alpha += 4;
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV420P10:
	{
		shared_ptr<Image> yuv = other->convert_pixel_format (dcp::YUV_TO_RGB_REC709, _pixel_format, false, false);
		dcp::Size const ts = size();
		dcp::Size const os = yuv->size();
		for (int ty = start_ty, oy = start_oy; ty < ts.height && oy < os.height; ++ty, ++oy) {
			int const hty = ty / 2;
			int const hoy = oy / 2;
			uint16_t* tY = ((uint16_t *) (data()[0] + (ty * stride()[0]))) + start_tx;
			uint16_t* tU = ((uint16_t *) (data()[1] + (hty * stride()[1]))) + start_tx / 2;
			uint16_t* tV = ((uint16_t *) (data()[2] + (hty * stride()[2]))) + start_tx / 2;
			uint16_t* oY = ((uint16_t *) (yuv->data()[0] + (oy * yuv->stride()[0]))) + start_ox;
			uint16_t* oU = ((uint16_t *) (yuv->data()[1] + (hoy * yuv->stride()[1]))) + start_ox / 2;
			uint16_t* oV = ((uint16_t *) (yuv->data()[2] + (hoy * yuv->stride()[2]))) + start_ox / 2;
			uint8_t* alpha = other->data()[0] + (oy * other->stride()[0]) + start_ox * 4;
			for (int tx = start_tx, ox = start_ox; tx < ts.width && ox < os.width; ++tx, ++ox) {
				float const a = float(alpha[3]) / 255;
				*tY = *oY * a + *tY * (1 - a);
				*tU = *oU * a + *tU * (1 - a);
				*tV = *oV * a + *tV * (1 - a);
				++tY;
				++oY;
				if (tx % 2) {
					++tU;
					++tV;
				}
				if (ox % 2) {
					++oU;
					++oV;
				}
				alpha += 4;
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV422P10LE:
	{
		shared_ptr<Image> yuv = other->convert_pixel_format (dcp::YUV_TO_RGB_REC709, _pixel_format, false, false);
		dcp::Size const ts = size();
		dcp::Size const os = yuv->size();
		for (int ty = start_ty, oy = start_oy; ty < ts.height && oy < os.height; ++ty, ++oy) {
			uint16_t* tY = ((uint16_t *) (data()[0] + (ty * stride()[0]))) + start_tx;
			uint16_t* tU = ((uint16_t *) (data()[1] + (ty * stride()[1]))) + start_tx / 2;
			uint16_t* tV = ((uint16_t *) (data()[2] + (ty * stride()[2]))) + start_tx / 2;
			uint16_t* oY = ((uint16_t *) (yuv->data()[0] + (oy * yuv->stride()[0]))) + start_ox;
			uint16_t* oU = ((uint16_t *) (yuv->data()[1] + (oy * yuv->stride()[1]))) + start_ox / 2;
			uint16_t* oV = ((uint16_t *) (yuv->data()[2] + (oy * yuv->stride()[2]))) + start_ox / 2;
			uint8_t* alpha = other->data()[0] + (oy * other->stride()[0]) + start_ox * 4;
			for (int tx = start_tx, ox = start_ox; tx < ts.width && ox < os.width; ++tx, ++ox) {
				float const a = float(alpha[3]) / 255;
				*tY = *oY * a + *tY * (1 - a);
				*tU = *oU * a + *tU * (1 - a);
				*tV = *oV * a + *tV * (1 - a);
				++tY;
				++oY;
				if (tx % 2) {
					++tU;
					++tV;
				}
				if (ox % 2) {
					++oU;
					++oV;
				}
				alpha += 4;
			}
		}
		break;
	}
	default:
		throw PixelFormatError ("alpha_blend()", _pixel_format);
	}
}

void
Image::copy (shared_ptr<const Image> other, Position<int> position)
{
	/* Only implemented for RGB24 onto RGB24 so far */
	DCPOMATIC_ASSERT (_pixel_format == AV_PIX_FMT_RGB24 && other->pixel_format() == AV_PIX_FMT_RGB24);
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
	for (int i = 0; i < planes(); ++i) {
		uint8_t* p = data()[i];
		int const lines = sample_size(i).height;
		for (int y = 0; y < lines; ++y) {
			socket->read (p, line_size()[i]);
			p += stride()[i];
		}
	}
}

void
Image::write_to_socket (shared_ptr<Socket> socket) const
{
	for (int i = 0; i < planes(); ++i) {
		uint8_t* p = data()[i];
		int const lines = sample_size(i).height;
		for (int y = 0; y < lines; ++y) {
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
		throw PixelFormatError ("bytes_per_pixel()", _pixel_format);
	}

	if (c >= planes()) {
		return 0;
	}

	float bpp[4] = { 0, 0, 0, 0 };

#ifdef DCPOMATIC_HAVE_AVCOMPONENTDESCRIPTOR_DEPTH_MINUS1
	bpp[0] = floor ((d->comp[0].depth_minus1 + 8) / 8);
	if (d->nb_components > 1) {
		bpp[1] = floor ((d->comp[1].depth_minus1 + 8) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 2) {
		bpp[2] = floor ((d->comp[2].depth_minus1 + 8) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 3) {
		bpp[3] = floor ((d->comp[3].depth_minus1 + 8) / 8) / pow (2.0f, d->log2_chroma_w);
	}
#else
	bpp[0] = floor ((d->comp[0].depth + 7) / 8);
	if (d->nb_components > 1) {
		bpp[1] = floor ((d->comp[1].depth + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 2) {
		bpp[2] = floor ((d->comp[2].depth + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
	if (d->nb_components > 3) {
		bpp[3] = floor ((d->comp[3].depth + 7) / 8) / pow (2.0f, d->log2_chroma_w);
	}
#endif

	if ((d->flags & AV_PIX_FMT_FLAG_PLANAR) == 0) {
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
 *  @param aligned true to make each row of this image aligned to a ALIGNMENT-byte boundary.
 */
Image::Image (AVPixelFormat p, dcp::Size s, bool aligned)
	: _size (s)
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

	for (int i = 0; i < planes(); ++i) {
		_line_size[i] = ceil (_size.width * bytes_per_pixel(i));
		_stride[i] = stride_round_up (i, _line_size, _aligned ? ALIGNMENT : 1);

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
		   so I'll just over-allocate by ALIGNMENT bytes and have done with it.  Empirical
		   testing suggests that it works.

		   In addition to these concerns, we may read/write as much as a whole extra line
		   at the end of each plane in cases where we are messing with offsets in order to
		   do pad or crop.  To solve this we over-allocate by an extra _stride[i] bytes.

		   As an example: we may write to images starting at an offset so we get some padding.
		   Hence we want to write in the following pattern:

		   block start   write start                                  line end
		   |..(padding)..|<------line-size------------->|..(padding)..|
		   |..(padding)..|<------line-size------------->|..(padding)..|
		   |..(padding)..|<------line-size------------->|..(padding)..|

		   where line-size is of the smaller (inter_size) image and the full padded line length is that of
		   out_size.  To get things to work we have to tell FFmpeg that the stride is that of out_size.
		   However some parts of FFmpeg (notably rgb48Toxyz12 in swscale.c) process data for the full
		   specified *stride*.  This does not matter until we get to the last line:

		   block start   write start                                  line end
		   |..(padding)..|<------line-size------------->|XXXwrittenXXX|
		   |XXXwrittenXXX|<------line-size------------->|XXXwrittenXXX|
		   |XXXwrittenXXX|<------line-size------------->|XXXwrittenXXXXXXwrittenXXX
		                                                               ^^^^ out of bounds
		*/
		_data[i] = (uint8_t *) wrapped_av_malloc (_stride[i] * (sample_size(i).height + 1) + ALIGNMENT);
#if HAVE_VALGRIND_MEMCHECK_H
		/* The data between the end of the line size and the stride is undefined but processed by
		   libswscale, causing lots of valgrind errors.  Mark it all defined to quell these errors.
		*/
		VALGRIND_MAKE_MEM_DEFINED (_data[i], _stride[i] * (sample_size(i).height + 1) + ALIGNMENT);
#endif
	}
}

Image::Image (Image const & other)
	: boost::enable_shared_from_this<Image>(other)
	, _size (other._size)
	, _pixel_format (other._pixel_format)
	, _aligned (other._aligned)
{
	allocate ();

	for (int i = 0; i < planes(); ++i) {
		uint8_t* p = _data[i];
		uint8_t* q = other._data[i];
		int const lines = sample_size(i).height;
		for (int j = 0; j < lines; ++j) {
			memcpy (p, q, _line_size[i]);
			p += stride()[i];
			q += other.stride()[i];
		}
	}
}

Image::Image (AVFrame* frame)
	: _size (frame->width, frame->height)
	, _pixel_format (static_cast<AVPixelFormat> (frame->format))
	, _aligned (true)
{
	allocate ();

	for (int i = 0; i < planes(); ++i) {
		uint8_t* p = _data[i];
		uint8_t* q = frame->data[i];
		int const lines = sample_size(i).height;
		for (int j = 0; j < lines; ++j) {
			memcpy (p, q, _line_size[i]);
			p += stride()[i];
			/* AVFrame's linesize is what we call `stride' */
			q += frame->linesize[i];
		}
	}
}

Image::Image (shared_ptr<const Image> other, bool aligned)
	: _size (other->_size)
	, _pixel_format (other->_pixel_format)
	, _aligned (aligned)
{
	allocate ();

	for (int i = 0; i < planes(); ++i) {
		DCPOMATIC_ASSERT (line_size()[i] == other->line_size()[i]);
		uint8_t* p = _data[i];
		uint8_t* q = other->data()[i];
		int const lines = sample_size(i).height;
		for (int j = 0; j < lines; ++j) {
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
	std::swap (_size, other._size);
	std::swap (_pixel_format, other._pixel_format);

	for (int i = 0; i < 4; ++i) {
		std::swap (_data[i], other._data[i]);
		std::swap (_line_size[i], other._line_size[i]);
		std::swap (_stride[i], other._stride[i]);
	}

	std::swap (_aligned, other._aligned);
}

Image::~Image ()
{
	for (int i = 0; i < planes(); ++i) {
		av_free (_data[i]);
	}

	av_free (_data);
	av_free (_line_size);
	av_free (_stride);
}

uint8_t * const *
Image::data () const
{
	return _data;
}

int const *
Image::line_size () const
{
	return _line_size;
}

int const *
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

bool
operator== (Image const & a, Image const & b)
{
	if (a.planes() != b.planes() || a.pixel_format() != b.pixel_format() || a.aligned() != b.aligned()) {
		return false;
	}

	for (int c = 0; c < a.planes(); ++c) {
		if (a.sample_size(c).height != b.sample_size(c).height || a.line_size()[c] != b.line_size()[c] || a.stride()[c] != b.stride()[c]) {
			return false;
		}

		uint8_t* p = a.data()[c];
		uint8_t* q = b.data()[c];
		int const lines = a.sample_size(c).height;
		for (int y = 0; y < lines; ++y) {
			if (memcmp (p, q, a.line_size()[c]) != 0) {
				return false;
			}

			p += a.stride()[c];
			q += b.stride()[c];
		}
	}

	return true;
}

/** Fade the image.
 *  @param f Amount to fade by; 0 is black, 1 is no fade.
 */
void
Image::fade (float f)
{
	/* U/V black value for 8-bit colour */
	static int const eight_bit_uv =    (1 << 7) - 1;
	/* U/V black value for 10-bit colour */
	static uint16_t const ten_bit_uv = (1 << 9) - 1;

	switch (_pixel_format) {
	case AV_PIX_FMT_YUV420P:
	{
		/* Y */
		uint8_t* p = data()[0];
		int const lines = sample_size(0).height;
		for (int y = 0; y < lines; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < line_size()[0]; ++x) {
				*q = int(float(*q) * f);
				++q;
			}
			p += stride()[0];
		}

		/* U, V */
		for (int c = 1; c < 3; ++c) {
			uint8_t* p = data()[c];
			int const lines = sample_size(c).height;
			for (int y = 0; y < lines; ++y) {
				uint8_t* q = p;
				for (int x = 0; x < line_size()[c]; ++x) {
					*q = eight_bit_uv + int((int(*q) - eight_bit_uv) * f);
					++q;
				}
				p += stride()[c];
			}
		}

		break;
	}

	case AV_PIX_FMT_RGB24:
	{
		/* 8-bit */
		uint8_t* p = data()[0];
		int const lines = sample_size(0).height;
		for (int y = 0; y < lines; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < line_size()[0]; ++x) {
				*q = int (float (*q) * f);
				++q;
			}
			p += stride()[0];
		}
		break;
	}

	case AV_PIX_FMT_XYZ12LE:
	case AV_PIX_FMT_RGB48LE:
		/* 16-bit little-endian */
		for (int c = 0; c < 3; ++c) {
			int const stride_pixels = stride()[c] / 2;
			int const line_size_pixels = line_size()[c] / 2;
			uint16_t* p = reinterpret_cast<uint16_t*> (data()[c]);
			int const lines = sample_size(c).height;
			for (int y = 0; y < lines; ++y) {
				uint16_t* q = p;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = int (float (*q) * f);
					++q;
				}
				p += stride_pixels;
			}
		}
		break;

	case AV_PIX_FMT_YUV422P10LE:
	{
		/* Y */
		{
			int const stride_pixels = stride()[0] / 2;
			int const line_size_pixels = line_size()[0] / 2;
			uint16_t* p = reinterpret_cast<uint16_t*> (data()[0]);
			int const lines = sample_size(0).height;
			for (int y = 0; y < lines; ++y) {
				uint16_t* q = p;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = int(float(*q) * f);
					++q;
				}
				p += stride_pixels;
			}
		}

		/* U, V */
		for (int c = 1; c < 3; ++c) {
			int const stride_pixels = stride()[c] / 2;
			int const line_size_pixels = line_size()[c] / 2;
			uint16_t* p = reinterpret_cast<uint16_t*> (data()[c]);
			int const lines = sample_size(c).height;
			for (int y = 0; y < lines; ++y) {
				uint16_t* q = p;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = ten_bit_uv + int((int(*q) - ten_bit_uv) * f);
					++q;
				}
				p += stride_pixels;
			}
		}
		break;

	}

	default:
		throw PixelFormatError ("fade()", _pixel_format);
	}
}

shared_ptr<const Image>
Image::ensure_aligned (shared_ptr<const Image> image)
{
	if (image->aligned()) {
		return image;
	}

	return shared_ptr<Image> (new Image (image, true));
}

size_t
Image::memory_used () const
{
	size_t m = 0;
	for (int i = 0; i < planes(); ++i) {
		m += _stride[i] * sample_size(i).height;
	}
	return m;
}

class Memory
{
public:
	Memory ()
		: data(0)
		, size(0)
	{}

	~Memory ()
	{
		free (data);
	}

	uint8_t* data;
	size_t size;
};

static void
png_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	Memory* mem = reinterpret_cast<Memory*>(png_get_io_ptr(png_ptr));
	size_t size = mem->size + length;

	if (mem->data) {
		mem->data = reinterpret_cast<uint8_t*>(realloc(mem->data, size));
	} else {
		mem->data = reinterpret_cast<uint8_t*>(malloc(size));
	}

	if (!mem->data) {
		throw EncodeError (N_("could not allocate memory for PNG"));
	}

	memcpy (mem->data + mem->size, data, length);
	mem->size += length;
}

static void
png_flush (png_structp)
{

}

static void
png_error_fn (png_structp png_ptr, char const * message)
{
	reinterpret_cast<Image*>(png_get_error_ptr(png_ptr))->png_error (message);
}

void
Image::png_error (char const * message)
{
	throw EncodeError (String::compose ("Error during PNG write: %1", message));
}

dcp::ArrayData
Image::as_png () const
{
	DCPOMATIC_ASSERT (bytes_per_pixel(0) == 4);
	DCPOMATIC_ASSERT (planes() == 1);
	if (pixel_format() != AV_PIX_FMT_RGBA) {
		return convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGBA, true, false)->as_png();
	}

	/* error handling? */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, reinterpret_cast<void*>(const_cast<Image*>(this)), png_error_fn, 0);
	if (!png_ptr) {
		throw EncodeError (N_("could not create PNG write struct"));
	}

	Memory state;

	png_set_write_fn (png_ptr, &state, png_write_data, png_flush);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		throw EncodeError (N_("could not create PNG info struct"));
	}

	png_set_IHDR (png_ptr, info_ptr, size().width, size().height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_byte ** row_pointers = reinterpret_cast<png_byte **>(png_malloc(png_ptr, size().height * sizeof(png_byte *)));
	for (int i = 0; i < size().height; ++i) {
		row_pointers[i] = (png_byte *) (data()[0] + i * stride()[0]);
	}

	png_write_info (png_ptr, info_ptr);
	png_write_image (png_ptr, row_pointers);
	png_write_end (png_ptr, info_ptr);

	png_destroy_write_struct (&png_ptr, &info_ptr);
	png_free (png_ptr, row_pointers);

	return dcp::ArrayData (state.data, state.size);
}


void
Image::video_range_to_full_range ()
{
	switch (_pixel_format) {
	case AV_PIX_FMT_RGB24:
	{
		float const factor = 256.0 / 219.0;
		uint8_t* p = data()[0];
		int const lines = sample_size(0).height;
		for (int y = 0; y < lines; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < line_size()[0]; ++x) {
				*q = int((*q - 16) * factor);
				++q;
			}
			p += stride()[0];
		}
		break;
	}
	case AV_PIX_FMT_GBRP12LE:
	{
		float const factor = 4096.0 / 3504.0;
		for (int c = 0; c < 3; ++c) {
			uint16_t* p = reinterpret_cast<uint16_t*>(data()[c]);
			int const lines = sample_size(c).height;
			for (int y = 0; y < lines; ++y) {
				uint16_t* q = p;
				int const line_size_pixels = line_size()[c] / 2;
				for (int x = 0; x < line_size_pixels; ++x) {
					*q = int((*q - 256) * factor);
					++q;
				}
			}
		}
		break;
	}
	default:
		throw PixelFormatError ("video_range_to_full_range()", _pixel_format);
	}
}

