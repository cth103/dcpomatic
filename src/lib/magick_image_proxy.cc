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
#include "magick_image_proxy.h"
#include "cross.h"
#include "exceptions.h"
#include "util.h"
#include "image.h"

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

MagickImageProxy::MagickImageProxy (boost::filesystem::path path)
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

MagickImageProxy::MagickImageProxy (shared_ptr<cxml::Node>, shared_ptr<Socket> socket)
{
	uint32_t const size = socket->read_uint32 ();
	uint8_t* data = new uint8_t[size];
	socket->read (data, size);
	_blob.update (data, size);
	delete[] data;
}

shared_ptr<Image>
MagickImageProxy::image (optional<dcp::NoteHandler>) const
{
	if (_image) {
		return _image;
	}

	Magick::Image* magick_image = 0;
	string error;
	try {
		magick_image = new Magick::Image (_blob);
	} catch (Magick::Exception& e) {
		error = e.what ();
	}

	if (!magick_image) {
		/* ImageMagick cannot auto-detect Targa files, it seems, so try here with an
		   explicit format.  I can't find it documented that passing a (0, 0) geometry
		   is allowed, but it seems to work.
		*/
		try {
			magick_image = new Magick::Image (_blob, Magick::Geometry (0, 0), "TGA");
		} catch (...) {

		}
	}

	if (!magick_image) {
		/* If we failed both an auto-detect and a forced-Targa we give the error from
		   the auto-detect.
		*/
		throw DecodeError (String::compose (_("Could not decode image file (%1)"), error));
	}

	dcp::Size size (magick_image->columns(), magick_image->rows());

	_image.reset (new Image (PIX_FMT_RGB24, size, true));

	/* Write line-by-line here as _image must be aligned, and write() cannot be told about strides */
	uint8_t* p = _image->data()[0];
	for (int i = 0; i < size.height; ++i) {
#ifdef DCPOMATIC_IMAGE_MAGICK
		using namespace MagickCore;
#else
		using namespace MagickLib;
#endif		
		magick_image->write (0, i, size.width, 1, "RGB", CharPixel, p);
		p += _image->stride()[0];
	}

	delete magick_image;

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

bool
MagickImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	shared_ptr<const MagickImageProxy> mp = dynamic_pointer_cast<const MagickImageProxy> (other);
	if (!mp) {
		return false;
	}

	if (_blob.length() != mp->_blob.length()) {
		return false;
	}
	
	return memcmp (_blob.data(), mp->_blob.data(), _blob.length()) == 0;
}
