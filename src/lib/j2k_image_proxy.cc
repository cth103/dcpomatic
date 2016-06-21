/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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
#include "raw_convert.h"
#include <dcp/openjpeg_image.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/colour_conversion.h>
#include <dcp/rgb_xyz.h>
#include <dcp/j2k.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/make_shared.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using boost::make_shared;
using dcp::Data;

/** Construct a J2KImageProxy from a JPEG2000 file */
J2KImageProxy::J2KImageProxy (boost::filesystem::path path, dcp::Size size)
	: _data (path)
	, _size (size)
{

}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::MonoPictureFrame> frame, dcp::Size size)
	: _data (frame->j2k_size ())
	, _size (size)
{
	memcpy (_data.data().get(), frame->j2k_data(), _data.size ());
}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::StereoPictureFrame> frame, dcp::Size size, dcp::Eye eye)
	: _size (size)
	, _eye (eye)
{
	switch (eye) {
	case dcp::EYE_LEFT:
		_data = Data (frame->left_j2k_size ());
		memcpy (_data.data().get(), frame->left_j2k_data(), _data.size ());
		break;
	case dcp::EYE_RIGHT:
		_data = Data (frame->right_j2k_size ());
		memcpy (_data.data().get(), frame->right_j2k_data(), _data.size ());
		break;
	}
}

J2KImageProxy::J2KImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	_size = dcp::Size (xml->number_child<int> ("Width"), xml->number_child<int> ("Height"));
	if (xml->optional_number_child<int> ("Eye")) {
		_eye = static_cast<dcp::Eye> (xml->number_child<int> ("Eye"));
	}
	_data = Data (xml->number_child<int> ("Size"));
	socket->read (_data.data().get (), _data.size ());
}

void
J2KImageProxy::ensure_j2k () const
{
	if (!_j2k) {
		_j2k = dcp::decompress_j2k (const_cast<uint8_t*> (_data.data().get()), _data.size (), 0);
	}
}

shared_ptr<Image>
J2KImageProxy::image (optional<dcp::NoteHandler>) const
{
	ensure_j2k ();

	if (_j2k->precision(0) < 12) {
		int const shift = 12 - _j2k->precision (0);
		for (int c = 0; c < 3; ++c) {
			int* p = _j2k->data (c);
			for (int y = 0; y < _j2k->size().height; ++y) {
				for (int x = 0; x < _j2k->size().width; ++x) {
					*p++ <<= shift;
				}
			}
		}
	}

	shared_ptr<Image> image = make_shared<Image> (pixel_format(), _size, true);

	/* Copy data in whatever format (sRGB or XYZ) into our Image; I'm assuming
	   the data is 12-bit either way.
	*/

	int p = 0;
	for (int y = 0; y < _j2k->size().height; ++y) {
		uint16_t* q = (uint16_t *) (image->data()[0] + y * image->stride()[0]);
		for (int x = 0; x < _j2k->size().width; ++x) {
			for (int c = 0; c < 3; ++c) {
				*q++ = _j2k->data(c)[p] << 4;
			}
			++p;
		}
	}

	return image;
}

void
J2KImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("J2K"));
	node->add_child("Width")->add_child_text (raw_convert<string> (_size.width));
	node->add_child("Height")->add_child_text (raw_convert<string> (_size.height));
	if (_eye) {
		node->add_child("Eye")->add_child_text (raw_convert<string> (_eye.get ()));
	}
	node->add_child("Size")->add_child_text (raw_convert<string> (_data.size ()));
}

void
J2KImageProxy::send_binary (shared_ptr<Socket> socket) const
{
	socket->write (_data.data().get(), _data.size());
}

bool
J2KImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	shared_ptr<const J2KImageProxy> jp = dynamic_pointer_cast<const J2KImageProxy> (other);
	if (!jp) {
		return false;
	}

	if (_data.size() != jp->_data.size()) {
		return false;
	}

	return memcmp (_data.data().get(), jp->_data.data().get(), _data.size()) == 0;
}

AVPixelFormat
J2KImageProxy::pixel_format () const
{
	ensure_j2k ();

	if (_j2k->srgb ()) {
		return AV_PIX_FMT_RGB48LE;
	}

	return AV_PIX_FMT_XYZ12LE;
}

J2KImageProxy::J2KImageProxy (Data data, dcp::Size size)
	: _data (data)
	, _size (size)
{

}
