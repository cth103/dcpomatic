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


#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "image.h"
#include <png.h>

#include "i18n.h"


using std::shared_ptr;


class Memory
{
public:
	Memory () {}
	Memory (Memory const&) = delete;
	Memory& operator= (Memory const&) = delete;

	~Memory ()
	{
		free (data);
	}

	uint8_t* data = nullptr;
	size_t size = 0;
};


static void
png_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto mem = reinterpret_cast<Memory*>(png_get_io_ptr(png_ptr));
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
png_error_fn (png_structp, char const * message)
{
	throw EncodeError (String::compose("Error during PNG write: %1", message));
}


dcp::ArrayData
image_as_png (shared_ptr<const Image> image)
{
	DCPOMATIC_ASSERT (image->bytes_per_pixel(0) == 4);
	DCPOMATIC_ASSERT (image->planes() == 1);
	if (image->pixel_format() != AV_PIX_FMT_RGBA) {
		return image_as_png(image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGBA, Image::Alignment::PADDED, false));
	}

	/* error handling? */
	auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, reinterpret_cast<void*>(const_cast<Image*>(image.get())), png_error_fn, 0);
	if (!png_ptr) {
		throw EncodeError (N_("could not create PNG write struct"));
	}

	Memory state;

	png_set_write_fn (png_ptr, &state, png_write_data, png_flush);

	auto info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		throw EncodeError (N_("could not create PNG info struct"));
	}

	int const width = image->size().width;
	int const height = image->size().height;

	png_set_IHDR (png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	auto row_pointers = reinterpret_cast<png_byte **>(png_malloc(png_ptr, image->size().height * sizeof(png_byte *)));
	auto const data = image->data()[0];
	auto const stride = image->stride()[0];
	for (int i = 0; i < height; ++i) {
		row_pointers[i] = reinterpret_cast<png_byte *>(data + i * stride);
	}

	png_write_info (png_ptr, info_ptr);
	png_write_image (png_ptr, row_pointers);
	png_write_end (png_ptr, info_ptr);

	png_destroy_write_struct (&png_ptr, &info_ptr);
	png_free (png_ptr, row_pointers);

	return dcp::ArrayData (state.data, state.size);
}


