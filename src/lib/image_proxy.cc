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

#include <dcp/util.h>
#include <dcp/raw_convert.h>
#include "image_proxy.h"
#include "raw_image_proxy.h"
#include "magick_image_proxy.h"
#include "j2k_image_proxy.h"
#include "image.h"
#include "exceptions.h"
#include "cross.h"

#include "i18n.h"

using std::cout;
using std::string;
using boost::shared_ptr;

shared_ptr<ImageProxy>
image_proxy_factory (shared_ptr<cxml::Node> xml, shared_ptr<Socket> socket)
{
	if (xml->string_child("Type") == N_("Raw")) {
		return shared_ptr<ImageProxy> (new RawImageProxy (xml, socket));
	} else if (xml->string_child("Type") == N_("Magick")) {
		return shared_ptr<MagickImageProxy> (new MagickImageProxy (xml, socket));
	} else if (xml->string_child("Type") == N_("J2K")) {
		return shared_ptr<J2KImageProxy> (new J2KImageProxy (xml, socket));
	}

	throw NetworkError (_("Unexpected image type received by server"));
}
