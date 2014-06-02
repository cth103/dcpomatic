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

	_image.reset (new Image (PIX_FMT_RGB24, size, true));
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

	using namespace MagickCore;
	
	uint8_t* p = _image->data()[0];
	for (int y = 0; y < size.height; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = magick_image->pixelColor (x, y);
			*q++ = c.redQuantum() * 255 / QuantumRange;
			*q++ = c.greenQuantum() * 255 / QuantumRange;
			*q++ = c.blueQuantum() * 255 / QuantumRange;
		}
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
