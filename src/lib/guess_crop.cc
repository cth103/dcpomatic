/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "decoder.h"
#include "decoder_factory.h"
#include "image_proxy.h"
#include "guess_crop.h"
#include "image.h"
#include "video_decoder.h"


using std::shared_ptr;
using namespace dcpomatic;


static Crop
guess_crop(shared_ptr<const Image> image, std::function<bool (int, int, int, bool)> predicate)
{
	auto const width = image->size().width;
	auto const height = image->size().height;

	auto crop = Crop{};

	for (auto y = 0; y < height; ++y) {
		if (predicate(0, y, width, true)) {
			crop.top = y;
			break;
		}
	}

	for (auto y = height - 1; y >= 0; --y) {
		if (predicate(0, y, width, true)) {
			crop.bottom = height - 1 - y;
			break;
		}
	}

	for (auto x = 0; x < width; ++x) {
		if (predicate(x, 0, height, false)) {
			crop.left = x;
			break;
		}
	}

	for (auto x = width - 1; x >= 0; --x) {
		if (predicate(x, 0, height, false)) {
			crop.right = width - 1 - x;
			break;
		}
	}

	return crop;
}


Crop
guess_crop_by_brightness(shared_ptr<const Image> image, double threshold)
{
	std::function<bool (int, int, int, bool)> image_in_line = [image, threshold](int start_x, int start_y, int pixels, bool rows) {
		double brightest = 0;

		switch (image->pixel_format()) {
		case AV_PIX_FMT_RGB24:
		case AV_PIX_FMT_RGBA:
		{
			uint8_t const* data = image->data()[0] + start_x * std::lround(image->bytes_per_pixel(0)) + start_y * image->stride()[0];
			for (int p = 0; p < pixels; ++p) {
				/* Averaging R, G and B */
				brightest = std::max(brightest, static_cast<double>(data[0] + data[1] + data[2]) / (3 * 256));
				data += rows ? 3 : image->stride()[0];
			}
			break;
		}
		case AV_PIX_FMT_YUV420P:
		{
			uint8_t const* data = image->data()[0] + start_x + start_y * image->stride()[0];
			for (int p = 0; p < pixels; ++p) {
				/* Just using Y */
				brightest = std::max(brightest, static_cast<double>(*data) / 256);
				data += rows ? 1 : image->stride()[0];
			}
			break;
		}
		case AV_PIX_FMT_YUV422P10LE:
		{
			uint16_t const* data = reinterpret_cast<uint16_t*>(image->data()[0] + (start_x * 2) + (start_y * image->stride()[0]));
			for (int p = 0; p < pixels; ++p) {
				/* Just using Y */
				brightest = std::max(brightest, static_cast<double>(*data) / 1024);
				data += rows ? 1 : (image->stride()[0] / 2);
			}
			break;
		}
		default:
			throw PixelFormatError("guess_crop_by_brightness()", image->pixel_format());
		}

		return brightest > threshold;
	};

	return guess_crop(image, image_in_line);
}


/** @param position Time within the content to get a video frame from when guessing the crop */
Crop
guess_crop_by_brightness(shared_ptr<const Film> film, shared_ptr<const Content> content, double threshold, ContentTime position)
{
	DCPOMATIC_ASSERT (content->video);

	auto decoder = decoder_factory (film, content, false, false, {});
	DCPOMATIC_ASSERT (decoder->video);

	bool done = false;
	auto crop = Crop{};

	auto handle_video = [&done, &crop, threshold](ContentVideo video) {
		crop = guess_crop_by_brightness(video.image->image(Image::Alignment::COMPACT).image, threshold);
		done = true;
	};

	decoder->video->Data.connect (handle_video);
	decoder->seek (position, false);

	int tries_left = 50;
	while (!done && tries_left >= 0) {
		decoder->pass();
		--tries_left;
	}

	return crop;
}


Crop
guess_crop_by_alpha(shared_ptr<const Image> image)
{
	std::function<bool (int, int, int, bool)> image_in_line = [image](int start_x, int start_y, int pixels, bool rows) {
		switch (image->pixel_format()) {
		case AV_PIX_FMT_RGBA:
		{
			int const increment = rows ? 3 : image->stride()[0];
			uint8_t const* data = image->data()[0] + start_x * std::lround(image->bytes_per_pixel(0)) + start_y * image->stride()[0];
			for (int p = 0; p < pixels; ++p) {
				if (data[3]) {
					return true;
				}
				data += increment;
			}
			return false;
		}
		default:
			throw PixelFormatError("guess_crop_by_alpha()", image->pixel_format());
		}
	};

	return guess_crop(image, image_in_line);
}

