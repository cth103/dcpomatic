/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <Magick++.h>
#include <dcp/util.h>
#include <dcp/raw_convert.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_frame.h>
#include "image_proxy.h"
#include "image.h"
#include "exceptions.h"
#include "cross.h"
#include "log.h"

#include "i18n.h"

#define LOG_TIMING(...) _log->microsecond_log (String::compose (__VA_ARGS__), Log::TYPE_TIMING);

using std::cout;
using std::string;
using std::stringstream;
using boost::shared_ptr;

ImageProxy::ImageProxy (shared_ptr<Log> log)
	: _log (log)
{

}

RawImageProxy::RawImageProxy (shared_ptr<Image> image, shared_ptr<Log> log)
	: ImageProxy (log)
	, _image (image)
{

}

RawImageProxy::RawImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket, shared_ptr<Log> log)
	: ImageProxy (log)
{
	dcp::Size size (
		xml->number_child<int> ("Width"), xml->number_child<int> ("Height")
		);

	_image.reset (new Image (static_cast<AVPixelFormat> (xml->number_child<int> ("PixelFormat")), size, true));
	_image->read_from_socket (socket);
}

shared_ptr<Image>
RawImageProxy::image () const
{
	return _image;
}

void
RawImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("Raw"));
	node->add_child("Width")->add_child_text (dcp::raw_convert<string> (_image->size().width));
	node->add_child("Height")->add_child_text (dcp::raw_convert<string> (_image->size().height));
	node->add_child("PixelFormat")->add_child_text (dcp::raw_convert<string> (_image->pixel_format ()));
}

void
RawImageProxy::send_binary (shared_ptr<Socket> socket) const
{
	_image->write_to_socket (socket);
}

MagickImageProxy::MagickImageProxy (boost::filesystem::path path, shared_ptr<Log> log)
	: ImageProxy (log)
{
	/* Read the file into a Blob */
	
	boost::uintmax_t const size = boost::filesystem::file_size (path);
	FILE* f = fopen_boost (path, "rb");
	if (!f) {
		throw OpenFileError (path);
	}
		
	uint8_t* data = new uint8_t[size];
	if (fread (data, 1, size, f) != size) {
		delete[] data;
		throw ReadFileError (path);
	}
	
	fclose (f);
	_blob.update (data, size);
	delete[] data;
}

MagickImageProxy::MagickImageProxy (shared_ptr<cxml::Node>, shared_ptr<Socket> socket, shared_ptr<Log> log)
	: ImageProxy (log)
{
	uint32_t const size = socket->read_uint32 ();
	uint8_t* data = new uint8_t[size];
	socket->read (data, size);
	_blob.update (data, size);
	delete[] data;
}

shared_ptr<Image>
MagickImageProxy::image () const
{
	if (_image) {
		return _image;
	}

	LOG_TIMING ("[%1] MagickImageProxy begins decode and convert of %2 bytes", boost::this_thread::get_id(), _blob.length());

	Magick::Image* magick_image = 0;
	try {
		magick_image = new Magick::Image (_blob);
	} catch (...) {
		throw DecodeError (_("Could not decode image file"));
	}

	dcp::Size size (magick_image->columns(), magick_image->rows());
	LOG_TIMING ("[%1] MagickImageProxy decode finished", boost::this_thread::get_id ());

	_image.reset (new Image (PIX_FMT_RGB24, size, true));

	/* Write line-by-line here as _image must be aligned, and write() cannot be told about strides */
	uint8_t* p = _image->data()[0];
	for (int i = 0; i < size.height; ++i) {
		using namespace MagickCore;
		magick_image->write (0, i, size.width, 1, "RGB", CharPixel, p);
		p += _image->stride()[0];
	}

	delete magick_image;

	LOG_TIMING ("[%1] MagickImageProxy completes decode and convert of %2 bytes", boost::this_thread::get_id(), _blob.length());

	return _image;
}

void
MagickImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("Magick"));
}

void
MagickImageProxy::send_binary (shared_ptr<Socket> socket) const
{
	socket->write (_blob.length ());
	socket->write ((uint8_t *) _blob.data (), _blob.length ());
}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::MonoPictureFrame> frame, dcp::Size size, shared_ptr<Log> log)
	: ImageProxy (log)
	, _mono (frame)
	, _size (size)
{
	
}

J2KImageProxy::J2KImageProxy (shared_ptr<const dcp::StereoPictureFrame> frame, dcp::Size size, dcp::Eye eye, shared_ptr<Log> log)
	: ImageProxy (log)
	, _stereo (frame)
	, _size (size)
	, _eye (eye)
{

}

J2KImageProxy::J2KImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket, shared_ptr<Log> log)
	: ImageProxy (log)
{
	_size = dcp::Size (xml->number_child<int> ("Width"), xml->number_child<int> ("Height"));
	if (xml->optional_number_child<int> ("Eye")) {
		_eye = static_cast<dcp::Eye> (xml->number_child<int> ("Eye"));
		int const left_size = xml->number_child<int> ("LeftSize");
		int const right_size = xml->number_child<int> ("RightSize");
		_stereo.reset (new dcp::StereoPictureFrame ());
		socket->read (_stereo->left_j2k_data(), left_size);
		socket->read (_stereo->right_j2k_data(), right_size);
	} else {
		int const size = xml->number_child<int> ("Size");
		_mono.reset (new dcp::MonoPictureFrame ());
		socket->read (_mono->j2k_data (), size);
	}
}

shared_ptr<Image>
J2KImageProxy::image ()
{
	shared_ptr<Image> image (new Image (PIX_FMT_RGB24, _size, false));

	if (_mono) {
		_mono->rgb_frame (image->data()[0]);
	} else {
		_stereo->rgb_frame (image->data()[0], _eye);
	}

	return shared_ptr<Image> (new Image (image, true));
}

void
J2KImageProxy::add_metadata (xmlpp::Node* node) const
{
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
		socket->write (_mono->j2k_data(), size);
	} else {
		socket->write (_stereo->left_j2k_data(), _stereo->left_j2k_size ());
		socket->write (_stereo->right_j2k_data(), _stereo->right_j2k_size ());
	}
}

shared_ptr<ImageProxy>
image_proxy_factory (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket, shared_ptr<Log> log)
{
	if (xml->string_child("Type") == N_("Raw")) {
		return shared_ptr<ImageProxy> (new RawImageProxy (xml, socket, log));
	} else if (xml->string_child("Type") == N_("Magick")) {
		return shared_ptr<MagickImageProxy> (new MagickImageProxy (xml, socket, log));
	}

	throw NetworkError (_("Unexpected image type received by server"));
}
