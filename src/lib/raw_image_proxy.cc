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


#include "raw_image_proxy.h"
#include "image.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
#include <dcp/util.h>
#include <libcxml/cxml.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS

#include "i18n.h"


using std::dynamic_pointer_cast;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::raw_convert;


RawImageProxy::RawImageProxy (shared_ptr<Image> image)
	: _image (image)
{

}


RawImageProxy::RawImageProxy (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	dcp::Size size (
		xml->number_child<int>("Width"), xml->number_child<int>("Height")
		);

	_image = make_shared<Image>(static_cast<AVPixelFormat>(xml->number_child<int>("PixelFormat")), size, Image::Alignment::PADDED);
	_image->read_from_socket (socket);
}


ImageProxy::Result
RawImageProxy::image (Image::Alignment alignment, optional<dcp::Size>) const
{
	/* This ensure_alignment could be wasteful */
	return Result (Image::ensure_alignment(_image, alignment), 0);
}


void
RawImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text(N_("Raw"));
	node->add_child("Width")->add_child_text(raw_convert<string>(_image->size().width));
	node->add_child("Height")->add_child_text(raw_convert<string>(_image->size().height));
	node->add_child("PixelFormat")->add_child_text(raw_convert<string>(static_cast<int>(_image->pixel_format())));
}


void
RawImageProxy::write_to_socket (shared_ptr<Socket> socket) const
{
	_image->write_to_socket (socket);
}


bool
RawImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	auto rp = dynamic_pointer_cast<const RawImageProxy> (other);
	if (!rp) {
		return false;
	}

	return (*_image.get()) == (*rp->image(_image->alignment()).image.get());
}


size_t
RawImageProxy::memory_used () const
{
	return _image->memory_used ();
}
