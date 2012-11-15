/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include "image.h"
#include "server.h"

using namespace boost;

int main ()
{
	uint8_t* rgb = new uint8_t[256];
	shared_ptr<Image> image (new Image (rgb, 0, 32, 32, 24));
	Server* s = new Server ("localhost", 2);
	image->encode_remotely (s);
	return 0;
}
