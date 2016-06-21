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

#include "image_proxy.h"
#include "raw_image_proxy.h"
#include "magick_image_proxy.h"
#include "j2k_image_proxy.h"
#include "image.h"
#include "exceptions.h"
#include "cross.h"
#include <dcp/util.h>
#include <libcxml/cxml.h>
#include <boost/make_shared.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::string;
using boost::shared_ptr;
using boost::make_shared;

shared_ptr<ImageProxy>
image_proxy_factory (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	if (xml->string_child("Type") == N_("Raw")) {
		return make_shared<RawImageProxy> (xml, socket);
	} else if (xml->string_child("Type") == N_("Magick")) {
		return make_shared<MagickImageProxy> (xml, socket);
	} else if (xml->string_child("Type") == N_("J2K")) {
		return make_shared<J2KImageProxy> (xml, socket);
	}

	throw NetworkError (_("Unexpected image type received by server"));
}
