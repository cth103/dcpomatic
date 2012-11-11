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

AudioStream::AudioStream (string t)
{
	stringstream n (t);
	n >> _id >> _sample_rate >> _channel_layout;

	for (int i = 0; i < 3; ++i) {
		size_t const s = t.find (' ');
		if (s != string::npos) {
			t = t.substr (s + 1);
		}
	}

	_name = t;
}

string
AudioStream::to_string () const
{
	return String::compose ("%1 %2 %3 %4", _id, _sample_rate, _channel_layout, _name);
}

SubtitleStream::SubtitleStream (string t)
{
	stringstream n (t);
	n >> _id;

	size_t const s = t.find (' ');
	if (s != string::npos) {
		_name = t.substr (s + 1);
	}
}

string
SubtitleStream::to_string () const
{
	return String::compose ("%1 %2", _id, _name);
}
