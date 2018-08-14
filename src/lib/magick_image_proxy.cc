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

#include "magick_image_proxy.h"
#include "cross.h"
#include "exceptions.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include "compose.hpp"
#include <Magick++.h>
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::pair;
using std::make_pair;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

MagickImageProxy::MagickImageProxy (boost::filesystem::path path)
	: _path (path)
{
	/* Read the file into a Blob */

	boost::uintmax_t const size = boost::filesystem::file_size (path);
	FILE* f = fopen_boost (path, "rb");
	if (!f) {
		throw OpenFileError (path, errno, true);
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

pair<shared_ptr<Image>, int>
MagickImageProxy::image (optional<dcp::NoteHandler>, optional<dcp::Size>) const
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_image) {
		return make_pair (_image, 0);
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
		if (_path) {
			throw DecodeError (String::compose (_("Could not decode image file %1 (%2)"), _path->string(), error));
		} else {
			throw DecodeError (String::compose (_("Could not decode image file (%1)"), error));
		}
	}

	unsigned char const * data = static_cast<unsigned char const *>(_blob.data());
	if (data[801] == 1 || magick_image->image()->colorspace == Magick::sRGBColorspace) {
		/* Either:
		   1.  The transfer characteristic in this file is "printing density"; in this case ImageMagick sets the colour space
		       to LogColorspace, or
		   2.  The file is sRGB.

		   Empirically we find that in these cases if we subsequently call colorSpace(Magick::RGBColorspace) the colours
		   are very wrong.  To prevent this, set the image colour space to RGB to stop the ::colorSpace call below doing
		   anything.  See #1123 and others.
		*/
		magick_image->image()->colorspace = Magick::RGBColorspace;
	}

	magick_image->colorSpace(Magick::RGBColorspace);

	dcp::Size size (magick_image->columns(), magick_image->rows());

	_image.reset (new Image (AV_PIX_FMT_RGB24, size, true));

	/* Write line-by-line here as _image must be aligned, and write() cannot be told about strides */
	uint8_t* p = _image->data()[0];
	for (int i = 0; i < size.height; ++i) {
#ifdef DCPOMATIC_HAVE_MAGICKCORE_NAMESPACE
		using namespace MagickCore;
#endif
#ifdef DCPOMATIC_HAVE_MAGICKLIB_NAMESPACE
		using namespace MagickLib;
#endif
		magick_image->write (0, i, size.width, 1, "RGB", CharPixel, p);
		p += _image->stride()[0];
	}

	delete magick_image;

	return make_pair (_image, 0);
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

AVPixelFormat
MagickImageProxy::pixel_format () const
{
	return AV_PIX_FMT_RGB24;
}

size_t
MagickImageProxy::memory_used () const
{
	size_t m = _blob.length();
	if (_image) {
		m += _image->memory_used();
	}
	return m;
}
