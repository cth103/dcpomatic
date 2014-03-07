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

#include "dcpomatic_time.h"

using std::ostream;

ContentTime::ContentTime (DCPTime d, FrameRateChange f)
	: Time (rint (d.get() * f.speed_up))
{

}

DCPTime min (DCPTime a, DCPTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}

ostream &
operator<< (ostream& s, ContentTime t)
{
	s << "[CONT " << t.get() << " " << t.seconds() << "s]";
	return s;
}

ostream &
operator<< (ostream& s, DCPTime t)
{
	s << "[DCP " << t.get() << " " << t.seconds() << "s]";
	return s;
}
