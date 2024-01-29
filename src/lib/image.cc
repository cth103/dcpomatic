/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "dcpomatic_socket.h"
#include "enum_indexed_vector.h"
#include "exceptions.h"
#include "image.h"
#include "maths_util.h"
#include "memory_util.h"
#include "rect.h"
#include "timer.h"
#include <dcp/rgb_xyz.h>
#include <dcp/transfer_function.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
LIBDCP_ENABLE_WARNINGS
#if HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif
#include <iostream>


#include "i18n.h"


using std::cerr;
using std::cout;
using std::list;
using std::make_shared;
using std::max;
using std::min;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using dcp::Size;


/** The memory alignment, in bytes, used for each row of an image if Alignment::PADDED is requested */
int constexpr ALIGNMENT = 64;

/* U/V black value for 8-bit colour */
static uint8_t const eight_bit_uv =	(1 << 7) - 1;
/* U/V black value for 9-bit colour */
static uint16_t const nine_bit_uv =	(1 << 8) - 1;
/* U/V black value for 10-bit colour */
static uint16_t const ten_bit_uv =	(1 << 9) - 1;
/* U/V black value for 16-bit colour */
static uint16_t const sixteen_bit_uv =	(1 << 15) - 1;


int
Image::vertical_factor (int n) const
{
	if (n == 0) {
		return 1;
	}

	auto d = av_pix_fmt_desc_get(_pixel_format);
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

	auto d = av_pix_fmt_desc_get(_pixel_format);
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
		lrint (ceil(static_cast<double>(size().width) / horizontal_factor(n))),
		lrint (ceil(static_cast<double>(size().height) / vertical_factor(n)))
		);
}


/** @return Number of planes */
int
Image::planes () const
{
	if (_pixel_format == AV_PIX_FMT_PAL8) {
		return 2;
	}

	auto d = av_pix_fmt_desc_get(_pixel_format);
	if (!d) {
		throw PixelFormatError ("planes()", _pixel_format);
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
	Alignment out_alignment,
	bool fast
	) const
{
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image is not padded.
	*/
	DCPOMATIC_ASSERT (alignment() == Alignment::PADDED);

	DCPOMATIC_ASSERT (out_size.width >= inter_size.width);
	DCPOMATIC_ASSERT (out_size.height >= inter_size.height);

	auto out = make_shared<Image>(out_format, out_size, out_alignment);
	out->make_black ();

	auto in_desc = av_pix_fmt_desc_get (_pixel_format);
	if (!in_desc) {
		throw PixelFormatError ("crop_scale_window()", _pixel_format);
	}

	/* Round down so that we crop only the number of pixels that is straightforward
	 * considering any subsampling.
	 */
	Crop corrected_crop(
		round_width_for_subsampling(crop.left, in_desc),
		round_width_for_subsampling(crop.right, in_desc),
		round_height_for_subsampling(crop.top, in_desc),
		round_height_for_subsampling(crop.bottom, in_desc)
		);

	/* Also check that we aren't cropping more image than there actually is */
	if ((corrected_crop.left + corrected_crop.right) >= (size().width - 4)) {
		corrected_crop.left = 0;
		corrected_crop.right = size().width - 4;
	}

	if ((corrected_crop.top + corrected_crop.bottom) >= (size().height - 4)) {
		corrected_crop.top = 0;
		corrected_crop.bottom = size().height - 4;
	}

	/* Size of the image after any crop */
	auto const cropped_size = corrected_crop.apply (size());

	/* Scale context for a scale from cropped_size to inter_size */
	auto scale_context = sws_getContext (
			cropped_size.width, cropped_size.height, pixel_format(),
			inter_size.width, inter_size.height, out_format,
			fast ? SWS_FAST_BILINEAR : SWS_BICUBIC, 0, 0, 0
		);

	if (!scale_context) {
		throw runtime_error (N_("Could not allocate SwsContext"));
	}

	DCPOMATIC_ASSERT (yuv_to_rgb < dcp::YUVToRGB::COUNT);
	EnumIndexedVector<int, dcp::YUVToRGB> lut;
	lut[dcp::YUVToRGB::REC601] = SWS_CS_ITU601;
	lut[dcp::YUVToRGB::REC709] = SWS_CS_ITU709;
	lut[dcp::YUVToRGB::REC2020] = SWS_CS_BT2020;

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
		sws_getCoefficients(lut[yuv_to_rgb]), video_range == VideoRange::VIDEO ? 0 : 1,
		sws_getCoefficients(lut[yuv_to_rgb]), out_video_range == VideoRange::VIDEO ? 0 : 1,
		0, 1 << 16, 1 << 16
		);

	/* Prepare input data pointers with crop */
	uint8_t* scale_in_data[planes()];
	for (int c = 0; c < planes(); ++c) {
		int const x = lrintf(bytes_per_pixel(c) * corrected_crop.left);
		scale_in_data[c] = data()[c] + x + stride()[c] * (corrected_crop.top / vertical_factor(c));
	}

	auto out_desc = av_pix_fmt_desc_get (out_format);
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

	/* There are some cases where there will be unwanted image data left in the image at this point:
	 *
	 * 1. When we are cropping without any scaling or pixel format conversion.
	 * 2. When we are scaling to certain sizes and placing the result into a larger
	 *    black frame.
	 *
	 * Clear out the sides of the image to take care of those cases.
	 */
	auto const pad = (out_size.width - inter_size.width) / 2;
	out->make_part_black(0, pad);
	out->make_part_black(corner.x + inter_size.width, pad);

	if (
		video_range == VideoRange::VIDEO &&
		out_video_range == VideoRange::FULL &&
		av_pix_fmt_desc_get(_pixel_format)->flags & AV_PIX_FMT_FLAG_RGB
	   ) {
		/* libswscale will not convert video range for RGB sources, so we have to do it ourselves */
		out->video_range_to_full_range ();
	}

	return out;
}


shared_ptr<Image>
Image::convert_pixel_format (dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, Alignment out_alignment, bool fast) const
{
	return scale(size(), yuv_to_rgb, out_format, out_alignment, fast);
}


/** @param out_size Size to scale to.
 *  @param yuv_to_rgb YUVToRGB transform transform to use, if required.
 *  @param out_format Output pixel format.
 *  @param out_alignment Output alignment.
 *  @param fast Try to be fast at the possible expense of quality; at present this means using
 *  fast bilinear rather than bicubic scaling.
 */
shared_ptr<Image>
Image::scale (dcp::Size out_size, dcp::YUVToRGB yuv_to_rgb, AVPixelFormat out_format, Alignment out_alignment, bool fast) const
{
	/* Empirical testing suggests that sws_scale() will crash if
	   the input image alignment is not PADDED.
	*/
	DCPOMATIC_ASSERT (alignment() == Alignment::PADDED);

	auto scaled = make_shared<Image>(out_format, out_size, out_alignment);
	auto scale_context = sws_getContext (
		size().width, size().height, pixel_format(),
		out_size.width, out_size.height, out_format,
		(fast ? SWS_FAST_BILINEAR : SWS_BICUBIC) | SWS_ACCURATE_RND, 0, 0, 0
		);

	DCPOMATIC_ASSERT (yuv_to_rgb < dcp::YUVToRGB::COUNT);
	EnumIndexedVector<int, dcp::YUVToRGB> lut;
	lut[dcp::YUVToRGB::REC601] = SWS_CS_ITU601;
	lut[dcp::YUVToRGB::REC709] = SWS_CS_ITU709;
	lut[dcp::YUVToRGB::REC2020] = SWS_CS_BT2020;

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
		sws_getCoefficients(lut[yuv_to_rgb]), 0,
		sws_getCoefficients(lut[yuv_to_rgb]), 0,
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
		auto p = reinterpret_cast<int16_t*> (data()[i]);
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
Image::make_part_black (int const start, int const width)
{
	auto y_part = [&]() {
		int const bpp = bytes_per_pixel(0);
		int const h = sample_size(0).height;
		int const s = stride()[0];
		auto p = data()[0];
		for (int y = 0; y < h; ++y) {
			memset (p + start * bpp, 0, width * bpp);
			p += s;
		}
	};

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
			memset (p + start * bpp, 0, width * bpp);
			p += s;
		}
		break;
	}
	case AV_PIX_FMT_YUV420P:
	{
		y_part ();
		for (int i = 1; i < 3; ++i) {
			auto p = data()[i];
			int const h = sample_size(i).height;
			for (int y = 0; y < h; ++y) {
				for (int x = start / 2; x < (start + width) / 2; ++x) {
					p[x] = eight_bit_uv;
				}
				p += stride()[i];
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV422P10LE:
	{
		y_part ();
		for (int i = 1; i < 3; ++i) {
			auto p = reinterpret_cast<int16_t*>(data()[i]);
			int const h = sample_size(i).height;
			for (int y = 0; y < h; ++y) {
				for (int x = start / 2; x < (start + width) / 2; ++x) {
					p[x] = ten_bit_uv;
				}
				p += stride()[i] / 2;
			}
		}
		break;
	}
	case AV_PIX_FMT_YUV444P10LE:
	{
		y_part();
		for (int i = 1; i < 3; ++i) {
			auto p = reinterpret_cast<int16_t*>(data()[i]);
			int const h = sample_size(i).height;
			for (int y = 0; y < h; ++y) {
				for (int x = start; x < (start + width); ++x) {
					p[x] = ten_bit_uv;
				}
				p += stride()[i] / 2;
			}
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
	if (_pixel_format != AV_PIX_FMT_BGRA && _pixel_format != AV_PIX_FMT_RGBA && _pixel_format != AV_PIX_FMT_RGBA64BE) {
		throw PixelFormatError ("make_transparent()", _pixel_format);
	}

	memset (data()[0], 0, sample_size(0).height * stride()[0]);
}


struct TargetParams
{
	int start_x;
	int start_y;
	dcp::Size size;
	uint8_t* const* data;
	int const* stride;
	int bpp;

	uint8_t* line_pointer(int y) const {
		return data[0] + y * stride[0] + start_x * bpp;
	}
};


/** Parameters of the other image (the one being blended onto the target) when target and other are RGB */
struct OtherRGBParams
{
	int start_x;
	int start_y;
	dcp::Size size;
	uint8_t* const* data;
	int const* stride;
	int bpp;

	uint8_t* line_pointer(int y) const {
		return data[0] + y * stride[0];
	}

	float alpha_divisor() const {
		return pow(2, bpp * 2) - 1;
	}
};


/** Parameters of the other image (the one being blended onto the target) when target and other are YUV */
struct OtherYUVParams
{
	int start_x;
	int start_y;
	dcp::Size size;
	uint8_t* const* data;
	int const* stride;

	uint8_t* const* alpha_data;
	int const* alpha_stride;
	int alpha_bpp;
};


template <class OtherType>
void
alpha_blend_onto_rgb24(TargetParams const& target, OtherRGBParams const& other, int red, int blue, std::function<float (OtherType*)> get, int value_divisor)
{
	/* Going onto RGB24.  First byte is red, second green, third blue */
	auto const alpha_divisor = other.alpha_divisor();
	for (int ty = target.start_y, oy = other.start_y; ty < target.size.height && oy < other.size.height; ++ty, ++oy) {
		auto tp = target.line_pointer(ty);
		auto op = reinterpret_cast<OtherType*>(other.line_pointer(oy));
		for (int tx = target.start_x, ox = other.start_x; tx < target.size.width && ox < other.size.width; ++tx, ++ox) {
			float const alpha = get(op + 3) / alpha_divisor;
			tp[0] = (get(op + red) / value_divisor) * alpha + tp[0] * (1 - alpha);
			tp[1] = (get(op + 1) / value_divisor) * alpha + tp[1] * (1 - alpha);
			tp[2] = (get(op + blue) / value_divisor) * alpha + tp[2] * (1 - alpha);

			tp += target.bpp;
			op += other.bpp / sizeof(OtherType);
		}
	}
}


template <class OtherType>
void
alpha_blend_onto_bgra(TargetParams const& target, OtherRGBParams const& other, int red, int blue, std::function<float (OtherType*)> get, int value_divisor)
{
	auto const alpha_divisor = other.alpha_divisor();
	for (int ty = target.start_y, oy = other.start_y; ty < target.size.height && oy < other.size.height; ++ty, ++oy) {
		auto tp = target.line_pointer(ty);
		auto op = reinterpret_cast<OtherType*>(other.line_pointer(oy));
		for (int tx = target.start_x, ox = other.start_x; tx < target.size.width && ox < other.size.width; ++tx, ++ox) {
			float const alpha = get(op + 3) / alpha_divisor;
			tp[0] = (get(op + blue) / value_divisor) * alpha + tp[0] * (1 - alpha);
			tp[1] = (get(op + 1) / value_divisor) * alpha + tp[1] * (1 - alpha);
			tp[2] = (get(op + red) / value_divisor) * alpha + tp[2] * (1 - alpha);
			tp[3] = (get(op + 3) / value_divisor) * alpha + tp[3] * (1 - alpha);

			tp += target.bpp;
			op += other.bpp / sizeof(OtherType);
		}
	}
}


template <class OtherType>
void
alpha_blend_onto_rgba(TargetParams const& target, OtherRGBParams const& other, int red, int blue, std::function<float (OtherType*)> get, int value_divisor)
{
	auto const alpha_divisor = other.alpha_divisor();
	for (int ty = target.start_y, oy = other.start_y; ty < target.size.height && oy < other.size.height; ++ty, ++oy) {
		auto tp = target.line_pointer(ty);
		auto op = reinterpret_cast<OtherType*>(other.line_pointer(oy));
		for (int tx = target.start_x, ox = other.start_x; tx < target.size.width && ox < other.size.width; ++tx, ++ox) {
			float const alpha = get(op + 3) / alpha_divisor;
			tp[0] = (get(op + red) / value_divisor) * alpha + tp[0] * (1 - alpha);
			tp[1] = (get(op + 1) / value_divisor) * alpha + tp[1] * (1 - alpha);
			tp[2] = (get(op + blue) / value_divisor) * alpha + tp[2] * (1 - alpha);
			tp[3] = (get(op + 3) / value_divisor) * alpha + tp[3] * (1 - alpha);

			tp += target.bpp;
			op += other.bpp / sizeof(OtherType);
		}
	}
}


template <class OtherType>
void
alpha_blend_onto_rgb48le(TargetParams const& target, OtherRGBParams const& other, int red, int blue, std::function<float (OtherType*)> get, int value_scale)
{
	auto const alpha_divisor = other.alpha_divisor();
	for (int ty = target.start_y, oy = other.start_y; ty < target.size.height && oy < other.size.height; ++ty, ++oy) {
		auto tp = reinterpret_cast<uint16_t*>(target.line_pointer(ty));
		auto op = reinterpret_cast<OtherType*>(other.line_pointer(oy));
		for (int tx = target.start_x, ox = other.start_x; tx < target.size.width && ox < other.size.width; ++tx, ++ox) {
			float const alpha = get(op + 3) / alpha_divisor;
			tp[0] = get(op + red) * value_scale * alpha + tp[0] * (1 - alpha);
			tp[1] = get(op + 1) * value_scale * alpha + tp[1] * (1 - alpha);
			tp[2] = get(op + blue) * value_scale * alpha + tp[2] * (1 - alpha);

			tp += target.bpp / 2;
			op += other.bpp / sizeof(OtherType);
		}
	}
}


template <class OtherType>
void
alpha_blend_onto_xyz12le(TargetParams const& target, OtherRGBParams const& other, int red, int blue, std::function<float (OtherType*)> get, int value_divisor)
{
	auto const alpha_divisor = other.alpha_divisor();
	auto conv = dcp::ColourConversion::srgb_to_xyz();
	double fast_matrix[9];
	dcp::combined_rgb_to_xyz(conv, fast_matrix);
	auto lut_in = conv.in()->double_lut(0, 1, 8, false);
	auto lut_out = conv.out()->int_lut(0, 1, 16, true, 65535);
	for (int ty = target.start_y, oy = other.start_y; ty < target.size.height && oy < other.size.height; ++ty, ++oy) {
		auto tp = reinterpret_cast<uint16_t*>(target.data[0] + ty * target.stride[0] + target.start_x * target.bpp);
		auto op = reinterpret_cast<OtherType*>(other.data[0] + oy * other.stride[0]);
		for (int tx = target.start_x, ox = other.start_x; tx < target.size.width && ox < other.size.width; ++tx, ++ox) {
			float const alpha = get(op + 3) / alpha_divisor;

			/* Convert sRGB to XYZ; op is BGRA.  First, input gamma LUT */
			double const r = lut_in[get(op + red) / value_divisor];
			double const g = lut_in[get(op + 1) / value_divisor];
			double const b = lut_in[get(op + blue) / value_divisor];

			/* RGB to XYZ, including Bradford transform and DCI companding */
			double const x = max(0.0, min(1.0, r * fast_matrix[0] + g * fast_matrix[1] + b * fast_matrix[2]));
			double const y = max(0.0, min(1.0, r * fast_matrix[3] + g * fast_matrix[4] + b * fast_matrix[5]));
			double const z = max(0.0, min(1.0, r * fast_matrix[6] + g * fast_matrix[7] + b * fast_matrix[8]));

			/* Out gamma LUT and blend */
			tp[0] = lut_out[lrint(x * 65535)] * alpha + tp[0] * (1 - alpha);
			tp[1] = lut_out[lrint(y * 65535)] * alpha + tp[1] * (1 - alpha);
			tp[2] = lut_out[lrint(z * 65535)] * alpha + tp[2] * (1 - alpha);

			tp += target.bpp / 2;
			op += other.bpp / sizeof(OtherType);
		}
	}
}


static
void
alpha_blend_onto_yuv420p(TargetParams const& target, OtherYUVParams const& other, std::function<float (uint8_t* data)> get_alpha)
{
	auto const ts = target.size;
	auto const os = other.size;
	for (int ty = target.start_y, oy = other.start_y; ty < ts.height && oy < os.height; ++ty, ++oy) {
		int const hty = ty / 2;
		int const hoy = oy / 2;
		uint8_t* tY = target.data[0] + (ty * target.stride[0]) + target.start_x;
		uint8_t* tU = target.data[1] + (hty * target.stride[1]) + target.start_x / 2;
		uint8_t* tV = target.data[2] + (hty * target.stride[2]) + target.start_x / 2;
		uint8_t* oY = other.data[0] + (oy * other.stride[0]) + other.start_x;
		uint8_t* oU = other.data[1] + (hoy * other.stride[1]) + other.start_x / 2;
		uint8_t* oV = other.data[2] + (hoy * other.stride[2]) + other.start_x / 2;
		uint8_t* alpha = other.alpha_data[0] + (oy * other.alpha_stride[0]) + other.start_x * other.alpha_bpp;
		for (int tx = target.start_x, ox = other.start_x; tx < ts.width && ox < os.width; ++tx, ++ox) {
			float const a = get_alpha(alpha);
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
			alpha += other.alpha_bpp;
		}
	}
}


static
void
alpha_blend_onto_yuv420p10(TargetParams const& target, OtherYUVParams const& other, std::function<float (uint8_t* data)> get_alpha)
{
	auto const ts = target.size;
	auto const os = other.size;
	for (int ty = target.start_y, oy = other.start_y; ty < ts.height && oy < os.height; ++ty, ++oy) {
		int const hty = ty / 2;
		int const hoy = oy / 2;
		uint16_t* tY = reinterpret_cast<uint16_t*>(target.data[0] + (ty * target.stride[0])) + target.start_x;
		uint16_t* tU = reinterpret_cast<uint16_t*>(target.data[1] + (hty * target.stride[1])) + target.start_x / 2;
		uint16_t* tV = reinterpret_cast<uint16_t*>(target.data[2] + (hty * target.stride[2])) + target.start_x / 2;
		uint16_t* oY = reinterpret_cast<uint16_t*>(other.data[0] + (oy * other.stride[0])) + other.start_x;
		uint16_t* oU = reinterpret_cast<uint16_t*>(other.data[1] + (hoy * other.stride[1])) + other.start_x / 2;
		uint16_t* oV = reinterpret_cast<uint16_t*>(other.data[2] + (hoy * other.stride[2])) + other.start_x / 2;
		uint8_t* alpha = other.alpha_data[0] + (oy * other.alpha_stride[0]) + other.start_x * other.alpha_bpp;
		for (int tx = target.start_x, ox = other.start_x; tx < ts.width && ox < os.width; ++tx, ++ox) {
			float const a = get_alpha(alpha);
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
			alpha += other.alpha_bpp;
		}
	}
}


static
void
alpha_blend_onto_yuv422p9or10le(TargetParams const& target, OtherYUVParams const& other, std::function<float (uint8_t* data)> get_alpha)
{
	auto const ts = target.size;
	auto const os = other.size;
	for (int ty = target.start_y, oy = other.start_y; ty < ts.height && oy < os.height; ++ty, ++oy) {
		uint16_t* tY = reinterpret_cast<uint16_t*>(target.data[0] + (ty * target.stride[0])) + target.start_x;
		uint16_t* tU = reinterpret_cast<uint16_t*>(target.data[1] + (ty * target.stride[1])) + target.start_x / 2;
		uint16_t* tV = reinterpret_cast<uint16_t*>(target.data[2] + (ty * target.stride[2])) + target.start_x / 2;
		uint16_t* oY = reinterpret_cast<uint16_t*>(other.data[0] + (oy * other.stride[0])) + other.start_x;
		uint16_t* oU = reinterpret_cast<uint16_t*>(other.data[1] + (oy * other.stride[1])) + other.start_x / 2;
		uint16_t* oV = reinterpret_cast<uint16_t*>(other.data[2] + (oy * other.stride[2])) + other.start_x / 2;
		uint8_t* alpha = other.alpha_data[0] + (oy * other.alpha_stride[0]) + other.start_x * other.alpha_bpp;
		for (int tx = target.start_x, ox = other.start_x; tx < ts.width && ox < os.width; ++tx, ++ox) {
			float const a = get_alpha(alpha);
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
			alpha += other.alpha_bpp;
		}
	}
}


static
void
alpha_blend_onto_yuv444p9or10le(TargetParams const& target, OtherYUVParams const& other, std::function<float (uint8_t* data)> get_alpha)
{
	auto const ts = target.size;
	auto const os = other.size;
	for (int ty = target.start_y, oy = other.start_y; ty < ts.height && oy < os.height; ++ty, ++oy) {
		uint16_t* tY = reinterpret_cast<uint16_t*>(target.data[0] + (ty * target.stride[0])) + target.start_x;
		uint16_t* tU = reinterpret_cast<uint16_t*>(target.data[1] + (ty * target.stride[1])) + target.start_x;
		uint16_t* tV = reinterpret_cast<uint16_t*>(target.data[2] + (ty * target.stride[2])) + target.start_x;
		uint16_t* oY = reinterpret_cast<uint16_t*>(other.data[0] + (oy * other.stride[0])) + other.start_x;
		uint16_t* oU = reinterpret_cast<uint16_t*>(other.data[1] + (oy * other.stride[1])) + other.start_x;
		uint16_t* oV = reinterpret_cast<uint16_t*>(other.data[2] + (oy * other.stride[2])) + other.start_x;
		uint8_t* alpha = other.alpha_data[0] + (oy * other.alpha_stride[0]) + other.start_x * other.alpha_bpp;
		for (int tx = target.start_x, ox = other.start_x; tx < ts.width && ox < os.width; ++tx, ++ox) {
			float const a = get_alpha(alpha);
			*tY = *oY * a + *tY * (1 - a);
			*tU = *oU * a + *tU * (1 - a);
			*tV = *oV * a + *tV * (1 - a);
			++tY;
			++oY;
			++tU;
			++tV;
			++oU;
			++oV;
			alpha += other.alpha_bpp;
		}
	}
}


void
Image::alpha_blend (shared_ptr<const Image> other, Position<int> position)
{
	DCPOMATIC_ASSERT(
		other->pixel_format() == AV_PIX_FMT_BGRA ||
		other->pixel_format() == AV_PIX_FMT_RGBA ||
		other->pixel_format() == AV_PIX_FMT_RGBA64BE
		);

	int const blue = other->pixel_format() == AV_PIX_FMT_BGRA ? 0 : 2;
	int const red = other->pixel_format() == AV_PIX_FMT_BGRA ? 2 : 0;

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

	TargetParams target_params = {
		start_tx,
		start_ty,
		size(),
		data(),
		stride(),
		0
	};

	OtherRGBParams other_rgb_params = {
		start_ox,
		start_oy,
		other->size(),
		other->data(),
		other->stride(),
		other->pixel_format() == AV_PIX_FMT_RGBA64BE ? 8 : 4
	};

	OtherYUVParams other_yuv_params = {
		start_ox,
		start_oy,
		other->size(),
		other->data(),
		other->stride(),
		nullptr,
		nullptr,
		other->pixel_format() == AV_PIX_FMT_RGBA64BE ? 8 : 4
	};

	auto byteswap = [](uint16_t* p) {
		return (*p >> 8) | ((*p & 0xff) << 8);
	};

	auto pass = [](uint8_t* p) {
		return *p;
	};

	auto get_alpha_64be = [](uint8_t* p) {
		return ((static_cast<int16_t>(p[6]) << 8) | p[7]) / 65535.0f;
	};

	auto get_alpha_byte = [](uint8_t* p) {
		return p[3] / 255.0f;
	};

	switch (_pixel_format) {
	case AV_PIX_FMT_RGB24:
		target_params.bpp = 3;
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_rgb24<uint16_t>(target_params, other_rgb_params, red, blue, byteswap, 256);
		} else {
			alpha_blend_onto_rgb24<uint8_t>(target_params, other_rgb_params, red, blue, pass, 1);
		}
		break;
	case AV_PIX_FMT_BGRA:
		target_params.bpp = 4;
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_bgra<uint16_t>(target_params, other_rgb_params, red, blue, byteswap, 256);
		} else {
			alpha_blend_onto_bgra<uint8_t>(target_params, other_rgb_params, red, blue, pass, 1);
		}
		break;
	case AV_PIX_FMT_RGBA:
		target_params.bpp = 4;
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_rgba<uint16_t>(target_params, other_rgb_params, red, blue, byteswap, 256);
		} else {
			alpha_blend_onto_rgba<uint8_t>(target_params, other_rgb_params, red, blue, pass, 1);
		}
		break;
	case AV_PIX_FMT_RGB48LE:
		target_params.bpp = 6;
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_rgb48le<uint16_t>(target_params, other_rgb_params, red, blue, byteswap, 1);
		} else {
			alpha_blend_onto_rgb48le<uint8_t>(target_params, other_rgb_params, red, blue, pass, 256);
		}
		break;
	case AV_PIX_FMT_XYZ12LE:
		target_params.bpp = 6;
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_xyz12le<uint16_t>(target_params, other_rgb_params, red, blue, byteswap, 256);
		} else {
			alpha_blend_onto_xyz12le<uint8_t>(target_params, other_rgb_params, red, blue, pass, 1);
		}
		break;
	case AV_PIX_FMT_YUV420P:
	{
		auto yuv = other->convert_pixel_format (dcp::YUVToRGB::REC709, _pixel_format, Alignment::COMPACT, false);
		other_yuv_params.data = yuv->data();
		other_yuv_params.stride = yuv->stride();
		other_yuv_params.alpha_data = other->data();
		other_yuv_params.alpha_stride = other->stride();
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_yuv420p(target_params, other_yuv_params, get_alpha_64be);
		} else {
			alpha_blend_onto_yuv420p(target_params, other_yuv_params, get_alpha_byte);
		}
		break;
	}
	case AV_PIX_FMT_YUV420P10:
	{
		auto yuv = other->convert_pixel_format (dcp::YUVToRGB::REC709, _pixel_format, Alignment::COMPACT, false);
		other_yuv_params.data = yuv->data();
		other_yuv_params.stride = yuv->stride();
		other_yuv_params.alpha_data = other->data();
		other_yuv_params.alpha_stride = other->stride();
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_yuv420p10(target_params, other_yuv_params, get_alpha_64be);
		} else {
			alpha_blend_onto_yuv420p10(target_params, other_yuv_params, get_alpha_byte);
		}
		break;
	}
	case AV_PIX_FMT_YUV422P9LE:
	case AV_PIX_FMT_YUV422P10LE:
	{
		auto yuv = other->convert_pixel_format (dcp::YUVToRGB::REC709, _pixel_format, Alignment::COMPACT, false);
		other_yuv_params.data = yuv->data();
		other_yuv_params.stride = yuv->stride();
		other_yuv_params.alpha_data = other->data();
		other_yuv_params.alpha_stride = other->stride();
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_yuv422p9or10le(target_params, other_yuv_params, get_alpha_64be);
		} else {
			alpha_blend_onto_yuv422p9or10le(target_params, other_yuv_params, get_alpha_byte);
		}
		break;
	}
	case AV_PIX_FMT_YUV444P9LE:
	case AV_PIX_FMT_YUV444P10LE:
	{
		auto yuv = other->convert_pixel_format (dcp::YUVToRGB::REC709, _pixel_format, Alignment::COMPACT, false);
		other_yuv_params.data = yuv->data();
		other_yuv_params.stride = yuv->stride();
		other_yuv_params.alpha_data = other->data();
		other_yuv_params.alpha_stride = other->stride();
		if (other->pixel_format() == AV_PIX_FMT_RGBA64BE) {
			alpha_blend_onto_yuv444p9or10le(target_params, other_yuv_params, get_alpha_64be);
		} else {
			alpha_blend_onto_yuv444p9or10le(target_params, other_yuv_params, get_alpha_byte);
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
	auto d = av_pix_fmt_desc_get(_pixel_format);
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
 *  @param alignment PADDED to make each row of this image aligned to a ALIGNMENT-byte boundary, otherwise COMPACT.
 */
Image::Image (AVPixelFormat p, dcp::Size s, Alignment alignment)
	: _size (s)
	, _pixel_format (p)
	, _alignment (alignment)
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

	auto stride_round_up = [](int stride, int t) {
		int const a = stride + (t - 1);
		return a - (a % t);
	};

	for (int i = 0; i < planes(); ++i) {
		_line_size[i] = ceil (_size.width * bytes_per_pixel(i));
		_stride[i] = stride_round_up (_line_size[i], _alignment == Alignment::PADDED ? ALIGNMENT : 1);

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
	: std::enable_shared_from_this<Image>(other)
	, _size (other._size)
	, _pixel_format (other._pixel_format)
	, _alignment (other._alignment)
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


Image::Image (AVFrame const * frame, Alignment alignment)
	: _size (frame->width, frame->height)
	, _pixel_format (static_cast<AVPixelFormat>(frame->format))
	, _alignment (alignment)
{
	DCPOMATIC_ASSERT (_pixel_format != AV_PIX_FMT_NONE);

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


Image::Image (shared_ptr<const Image> other, Alignment alignment)
	: _size (other->_size)
	, _pixel_format (other->_pixel_format)
	, _alignment (alignment)
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

	std::swap (_alignment, other._alignment);
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


Image::Alignment
Image::alignment () const
{
	return _alignment;
}


PositionImage
merge (list<PositionImage> images, Image::Alignment alignment)
{
	if (images.empty ()) {
		return {};
	}

	if (images.size() == 1) {
		images.front().image = Image::ensure_alignment(images.front().image, alignment);
		return images.front();
	}

	dcpomatic::Rect<int> all (images.front().position, images.front().image->size().width, images.front().image->size().height);
	for (auto const& i: images) {
		all.extend (dcpomatic::Rect<int>(i.position, i.image->size().width, i.image->size().height));
	}

	auto merged = make_shared<Image>(images.front().image->pixel_format(), dcp::Size(all.width, all.height), alignment);
	merged->make_transparent ();
	for (auto const& i: images) {
		merged->alpha_blend (i.image, i.position - all.position());
	}

	return PositionImage (merged, all.position ());
}


bool
operator== (Image const & a, Image const & b)
{
	if (a.planes() != b.planes() || a.pixel_format() != b.pixel_format() || a.alignment() != b.alignment()) {
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
Image::ensure_alignment (shared_ptr<const Image> image, Image::Alignment alignment)
{
	if (image->alignment() == alignment) {
		return image;
	}

	return make_shared<Image>(image, alignment);
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
				*q = clamp(lrintf((*q - 16) * factor), 0L, 255L);
				++q;
			}
			p += stride()[0];
		}
		break;
	}
	case AV_PIX_FMT_RGB48LE:
	{
		float const factor = 65536.0 / 56064.0;
		uint16_t* p = reinterpret_cast<uint16_t*>(data()[0]);
		int const lines = sample_size(0).height;
		for (int y = 0; y < lines; ++y) {
			uint16_t* q = p;
			int const line_size_pixels = line_size()[0] / 2;
			for (int x = 0; x < line_size_pixels; ++x) {
				*q = clamp(lrintf((*q - 4096) * factor), 0L, 65535L);
				++q;
			}
			p += stride()[0] / 2;
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
					*q = clamp(lrintf((*q - 256) * factor), 0L, 4095L);
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

