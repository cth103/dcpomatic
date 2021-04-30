/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "j2k_image_proxy.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include "dcpomatic_assert.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
#include <dcp/openjpeg_image.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/colour_conversion.h>
#include <dcp/rgb_xyz.h>
#include <dcp/j2k_transcode.h>
#include <libcxml/cxml.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::make_pair;
using std::make_shared;
using std::max;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::ArrayData;
using dcp::raw_convert;


/** Construct a J2KImageProxy from a JPEG2000 file */
J2KImageProxy::J2KImageProxy (boost::filesystem::path path, dcp::Size size, AVPixelFormat pixel_format)
	: _data (new dcp::ArrayData(path))
	, _size (size)
	, _pixel_format (pixel_format)
	, _error (false)
{
	/* ::image assumes 16bpp */
	DCPOMATIC_ASSERT (_pixel_format == AV_PIX_FMT_RGB48 || _pixel_format == AV_PIX_FMT_XYZ12LE);
}


J2KImageProxy::J2KImageProxy (
	shared_ptr<const dcp::MonoPictureFrame> frame,
	dcp::Size size,
	AVPixelFormat pixel_format,
	optional<int> forced_reduction
	)
	: _data (frame)
	, _size (size)
	, _pixel_format (pixel_format)
	, _forced_reduction (forced_reduction)
	, _error (false)
{
	/* ::image assumes 16bpp */
	DCPOMATIC_ASSERT (_pixel_format == AV_PIX_FMT_RGB48 || _pixel_format == AV_PIX_FMT_XYZ12LE);
}


J2KImageProxy::J2KImageProxy (
	shared_ptr<const dcp::StereoPictureFrame> frame,
	dcp::Size size,
	dcp::Eye eye,
	AVPixelFormat pixel_format,
	optional<int> forced_reduction
	)
	: _data (eye == dcp::Eye::LEFT ? frame->left() : frame->right())
	, _size (size)
	, _eye (eye)
	, _pixel_format (pixel_format)
	, _forced_reduction (forced_reduction)
	, _error (false)
{
	/* ::image assumes 16bpp */
	DCPOMATIC_ASSERT (_pixel_format == AV_PIX_FMT_RGB48 || _pixel_format == AV_PIX_FMT_XYZ12LE);
}


J2KImageProxy::J2KImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
	: _error (false)
{
	_size = dcp::Size (xml->number_child<int>("Width"), xml->number_child<int>("Height"));
	if (xml->optional_number_child<int>("Eye")) {
		_eye = static_cast<dcp::Eye>(xml->number_child<int>("Eye"));
	}
	shared_ptr<ArrayData> data(new ArrayData(xml->number_child<int>("Size")));
	/* This only matters when we are using J2KImageProxy for the preview, which
	   will never use this constructor (which is only used for passing data to
	   encode servers).  So we can put anything in here.  It's a bit of a hack.
	*/
	_pixel_format = AV_PIX_FMT_XYZ12LE;
	socket->read (data->data(), data->size());
	_data = data;
}


int
J2KImageProxy::prepare (optional<dcp::Size> target_size) const
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_image && target_size == _target_size) {
		DCPOMATIC_ASSERT (_reduce);
		return *_reduce;
	}

	int reduce = 0;

	if (_forced_reduction) {
		reduce = *_forced_reduction;
	} else {
		while (target_size && (_size.width / pow(2, reduce)) > target_size->width && (_size.height / pow(2, reduce)) > target_size->height) {
			++reduce;
		}

		--reduce;
		reduce = max (0, reduce);
	}

	try {
		/* XXX: should check that potentially trashing _data here doesn't matter */
		auto decompressed = dcp::decompress_j2k (const_cast<uint8_t*>(_data->data()), _data->size(), reduce);
		_image.reset (new Image (_pixel_format, decompressed->size(), true));

		int const shift = 16 - decompressed->precision (0);

		/* Copy data in whatever format (sRGB or XYZ) into our Image; I'm assuming
		   the data is 12-bit either way.
		   */

		int const width = decompressed->size().width;

		int p = 0;
		int* decomp_0 = decompressed->data (0);
		int* decomp_1 = decompressed->data (1);
		int* decomp_2 = decompressed->data (2);
		for (int y = 0; y < decompressed->size().height; ++y) {
			auto q = reinterpret_cast<uint16_t *>(_image->data()[0] + y * _image->stride()[0]);
			for (int x = 0; x < width; ++x) {
				*q++ = decomp_0[p] << shift;
				*q++ = decomp_1[p] << shift;
				*q++ = decomp_2[p] << shift;
				++p;
			}
		}
	} catch (dcp::J2KDecompressionError& e) {
		_image = make_shared<Image>(_pixel_format, _size, true);
		_image->make_black ();
		_error = true;
	}

	_target_size = target_size;
	_reduce = reduce;

	return reduce;
}


ImageProxy::Result
J2KImageProxy::image (optional<dcp::Size> target_size) const
{
	int const r = prepare (target_size);

	/* I think this is safe without a lock on mutex.  _image is guaranteed to be
	   set up when prepare() has happened.
	*/
	return Result (_image, r, _error);
}


void
J2KImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text(N_("J2K"));
	node->add_child("Width")->add_child_text(raw_convert<string>(_size.width));
	node->add_child("Height")->add_child_text(raw_convert<string>(_size.height));
	if (_eye) {
		node->add_child("Eye")->add_child_text(raw_convert<string>(static_cast<int>(_eye.get())));
	}
	node->add_child("Size")->add_child_text(raw_convert<string>(_data->size()));
}


void
J2KImageProxy::write_to_socket (shared_ptr<Socket> socket) const
{
	socket->write (_data->data(), _data->size());
}


bool
J2KImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	auto jp = dynamic_pointer_cast<const J2KImageProxy>(other);
	if (!jp) {
		return false;
	}

	return *_data == *jp->_data;
}


J2KImageProxy::J2KImageProxy (ArrayData data, dcp::Size size, AVPixelFormat pixel_format)
	: _data (new ArrayData(data))
	, _size (size)
	, _pixel_format (pixel_format)
	, _error (false)
{
	/* ::image assumes 16bpp */
	DCPOMATIC_ASSERT (_pixel_format == AV_PIX_FMT_RGB48 || _pixel_format == AV_PIX_FMT_XYZ12LE);
}


size_t
J2KImageProxy::memory_used () const
{
	size_t m = _data->size();
	if (_image) {
		/* 3 components, 16-bits per pixel */
		m += 3 * 2 * _image->size().width * _image->size().height;
	}
	return m;
}
