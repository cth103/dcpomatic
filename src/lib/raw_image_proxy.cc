/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "raw_image_proxy.h"
#include "image.h"
#include <dcp/raw_convert.h>
#include <dcp/util.h>
#include <libcxml/cxml.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using dcp::raw_convert;

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
RawImageProxy::image (optional<dcp::NoteHandler>, optional<dcp::Size>) const
{
	return _image;
}

void
RawImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("Raw"));
	node->add_child("Width")->add_child_text (raw_convert<string> (_image->size().width));
	node->add_child("Height")->add_child_text (raw_convert<string> (_image->size().height));
	node->add_child("PixelFormat")->add_child_text (raw_convert<string> (static_cast<int> (_image->pixel_format ())));
}

void
RawImageProxy::send_binary (shared_ptr<Socket> socket) const
{
	_image->write_to_socket (socket);
}

bool
RawImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	shared_ptr<const RawImageProxy> rp = dynamic_pointer_cast<const RawImageProxy> (other);
	if (!rp) {
		return false;
	}

	return (*_image.get()) == (*rp->image().get());
}

AVPixelFormat
RawImageProxy::pixel_format () const
{
	return _image->pixel_format ();
}

size_t
RawImageProxy::memory_used () const
{
	return _image->memory_used ();
}
