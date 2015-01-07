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

extern "C" {
#include <libavutil/pixfmt.h>
}
#include <libcxml/cxml.h>
#include <dcp/util.h>
#include <dcp/raw_convert.h>
#include "raw_image_proxy.h"
#include "image.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;
using boost::optional;

RawImageProxy::RawImageProxy (shared_ptr<Image> image)
	: _image (image)
{

}

RawImageProxy::RawImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	dcp::Size size (
		xml->number_child<int> ("Width"), xml->number_child<int> ("Height")
		);

	_image.reset (new Image (static_cast<AVPixelFormat> (xml->number_child<int> ("PixelFormat")), size, true));
	_image->read_from_socket (socket);
}

shared_ptr<Image>
RawImageProxy::image (optional<dcp::NoteHandler>) const
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
