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

#include <sstream>
#include "compose.hpp"
#include "stream.h"

using namespace std;

Stream::Stream (string t)
{
	stringstream n (t);
	n >> id;
	
	size_t const s = t.find (' ');
	if (s != string::npos) {
		name = t.substr (s + 1);
	}
}

string
Stream::to_string () const
{
	return String::compose ("%1 %2", id, name);
}
