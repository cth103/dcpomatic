/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "j2k_image_proxy.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include "encoded_data.h"
#include <dcp/raw_convert.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/colour_conversion.h>
#include <dcp/rgb_xyz.h>
#include <libcxml/cxml.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;
using boost::optional;

/** Construct a J2KImageProxy from a JPEG2000 file */
J2KImageProxy::J2KImageProxy (boost::filesystem::path path, dcp::Size size)
	: _mono (new dcp::MonoPictureFrame (path))
	, _size (size)
{

}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::MonoPictureFrame> frame, dcp::Size size)
	: _mono (frame)
	, _size (size)
{
	
}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::StereoPictureFrame> frame, dcp::Size size, dcp::Eye eye)
	: _stereo (frame)
	, _size (size)
	, _eye (eye)
{

}

J2KImageProxy::J2KImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	_size = dcp::Size (xml->number_child<int> ("Width"), xml->number_child<int> ("Height"));
	if (xml->optional_number_child<int> ("Eye")) {
		_eye = static_cast<dcp::Eye> (xml->number_child<int> ("Eye"));
		int const left_size = xml->number_child<int> ("LeftSize");
		int const right_size = xml->number_child<int> ("RightSize");
		shared_ptr<dcp::StereoPictureFrame> f (new dcp::StereoPictureFrame ());
		socket->read (f->left_j2k_data(), left_size);
		socket->read (f->right_j2k_data(), right_size);
		_stereo = f;
	} else {
		int const size = xml->number_child<int> ("Size");
		shared_ptr<dcp::MonoPictureFrame> f (new dcp::MonoPictureFrame ());
		socket->read (f->j2k_data (), size);
		_mono = f;
	}
}

shared_ptr<Image>
J2KImageProxy::image (optional<dcp::NoteHandler> note) const
{
	shared_ptr<Image> image (new Image (PIX_FMT_RGB48LE, _size, true));

	if (_mono) {
		dcp::xyz_to_rgb (_mono->xyz_image (), dcp::ColourConversion::xyz_to_srgb(), image->data()[0], image->stride()[0], note);
	} else {
		dcp::xyz_to_rgb (_stereo->xyz_image (_eye), dcp::ColourConversion::xyz_to_srgb(), image->data()[0], image->stride()[0], note);
	}

	return image;
}

void
J2KImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("J2K"));
	node->add_child("Width")->add_child_text (dcp::raw_convert<string> (_size.width));
	node->add_child("Height")->add_child_text (dcp::raw_convert<string> (_size.height));
	if (_stereo) {
		node->add_child("Eye")->add_child_text (dcp::raw_convert<string> (_eye));
		node->add_child("LeftSize")->add_child_text (dcp::raw_convert<string> (_stereo->left_j2k_size ()));
		node->add_child("RightSize")->add_child_text (dcp::raw_convert<string> (_stereo->right_j2k_size ()));
	} else {
		node->add_child("Size")->add_child_text (dcp::raw_convert<string> (_mono->j2k_size ()));
	}
}

void
J2KImageProxy::send_binary (shared_ptr<Socket> socket) const
{
	if (_mono) {
		socket->write (_mono->j2k_data(), _mono->j2k_size ());
	} else {
		socket->write (_stereo->left_j2k_data(), _stereo->left_j2k_size ());
		socket->write (_stereo->right_j2k_data(), _stereo->right_j2k_size ());
	}
}

shared_ptr<EncodedData>
J2KImageProxy::j2k () const
{
	if (_mono) {
		return shared_ptr<EncodedData> (new EncodedData (_mono->j2k_data(), _mono->j2k_size()));
	} else {
		if (_eye == dcp::EYE_LEFT) {
			return shared_ptr<EncodedData> (new EncodedData (_stereo->left_j2k_data(), _stereo->left_j2k_size()));
		} else {
			return shared_ptr<EncodedData> (new EncodedData (_stereo->right_j2k_data(), _stereo->right_j2k_size()));
		}
	}
}
