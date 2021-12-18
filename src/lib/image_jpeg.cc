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
#include <jpeglib.h>

#include "i18n.h"


using std::shared_ptr;


struct destination_mgr
{
	jpeg_destination_mgr pub;
	std::vector<uint8_t>* data;
};


static void
init_destination (j_compress_ptr)
{

}


/* Empty the output buffer (i.e. make more space for data); we'll make
 * the output buffer bigger instead.
 */
static boolean
empty_output_buffer (j_compress_ptr state)
{
	auto dest = reinterpret_cast<destination_mgr*> (state->dest);

	auto const old_size = dest->data->size();
	dest->data->resize (old_size * 2);

	dest->pub.next_output_byte = dest->data->data() + old_size;
	dest->pub.free_in_buffer = old_size;

	return TRUE;
}


static void
term_destination (j_compress_ptr state)
{
	auto dest = reinterpret_cast<destination_mgr*> (state->dest);

	dest->data->resize (dest->data->size() - dest->pub.free_in_buffer);
}


void
error_exit (j_common_ptr)
{
	throw EncodeError (N_("JPEG encoding error"));
}


dcp::ArrayData
image_as_jpeg (shared_ptr<const Image> image, int quality)
{
	if (image->pixel_format() != AV_PIX_FMT_RGB24) {
		return image_as_jpeg(
			Image::ensure_alignment(image, Image::Alignment::PADDED)->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::PADDED, false),
			quality
			);
	}

	struct jpeg_compress_struct compress;
	struct jpeg_error_mgr error;

	compress.err = jpeg_std_error (&error);
	error.error_exit = error_exit;
	jpeg_create_compress (&compress);

	auto mgr = new destination_mgr;
	compress.dest = reinterpret_cast<jpeg_destination_mgr*>(mgr);

	mgr->pub.init_destination = init_destination;
	mgr->pub.empty_output_buffer = empty_output_buffer;
	mgr->pub.term_destination = term_destination;

	std::vector<uint8_t> data(4096);
	mgr->data = &data;
	mgr->pub.next_output_byte = data.data();
	mgr->pub.free_in_buffer = data.size();

	compress.image_width = image->size().width;
	compress.image_height = image->size().height;
	compress.input_components = 3;
	compress.in_color_space = JCS_RGB;

	jpeg_set_defaults (&compress);
	jpeg_set_quality (&compress, quality, TRUE);

	jpeg_start_compress (&compress, TRUE);

	JSAMPROW row[1];
	auto const source_data = image->data()[0];
	auto const source_stride = image->stride()[0];
	for (auto y = 0U; y < compress.image_height; ++y) {
		row[0] = source_data + y * source_stride;
		jpeg_write_scanlines (&compress, row, 1);
	}

	jpeg_finish_compress (&compress);
	delete mgr;
	jpeg_destroy_compress (&compress);

	return dcp::ArrayData (data.data(), data.size());
}


