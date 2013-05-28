/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include <libdcp/types.h>
#include "container.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using std::vector;

vector<Container const *> Container::_containers;

void
Container::setup_containers ()
{
	_containers.push_back (new Container (float(1285) / 1080, "119", _("1.19"), "F"));
	_containers.push_back (new Container (float(1436) / 1080, "133", _("4:3"), "F"));
	_containers.push_back (new Container (float(1480) / 1080, "137", _("Academy"), "F"));
	_containers.push_back (new Container (float(1485) / 1080, "138", _("1.375"), "F"));
	_containers.push_back (new Container (float(1793) / 1080, "166", _("1.66"), "F"));
	_containers.push_back (new Container (float(1920) / 1080, "178", _("16:9"), "F"));
	_containers.push_back (new Container (float(1998) / 1080, "185", _("Flat"), "F"));
	_containers.push_back (new Container (float(2048) /  858, "239", _("Scope"), "S"));
	_containers.push_back (new Container (float(2048) / 1080, "full-frame", _("Full frame"), "C"));
}

/** @return A name to be presented to the user */
string
Container::name () const
{
	return _nickname;
}

Container const *
Container::from_id (string i)
{
	vector<Container const *>::iterator j = _containers.begin ();
	while (j != _containers.end() && (*j)->id() != i) {
		++j;
	}

	if (j == _containers.end ()) {
		return 0;
	}

	return *j;
}

libdcp::Size
Container::size (libdcp::Size full_frame) const
{
	if (_ratio < static_cast<float>(full_frame.width) / full_frame.height) {
		return libdcp::Size (full_frame.height * _ratio, full_frame.height);
	} else {
		return libdcp::Size (full_frame.width, full_frame.width / _ratio);
	}

	return libdcp::Size ();
}
