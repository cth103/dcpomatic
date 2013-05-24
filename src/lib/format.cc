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

/** @file src/format.cc
 *  @brief Class to describe a format (aspect ratio) that a Film should
 *  be shown in.
 */

#include <sstream>
#include <cstdlib>
#include <cassert>
#include <iomanip>
#include <iostream>
#include "format.h"
#include "film.h"
#include "playlist.h"

#include "i18n.h"

using std::string;
using std::setprecision;
using std::stringstream;
using std::vector;
using boost::shared_ptr;
using libdcp::Size;

vector<Format const *> Format::_formats;

/** @return A name to be presented to the user */
string
Format::name () const
{
	stringstream s;
	if (!_nickname.empty ()) {
		s << _nickname << N_(" (");
	}

	s << setprecision(3) << ratio() << N_(":1");

	if (!_nickname.empty ()) {
		s << N_(")");
	}

	return s.str ();
}

/** Fill our _formats vector with all available formats */
void
Format::setup_formats ()
{
	/// TRANSLATORS: these are film picture aspect ratios; "Academy" means 1.37, "Flat" 1.85 and "Scope" 2.39.
	_formats.push_back (new Format (libdcp::Size (1285, 1080), "119", _("1.19")));
	_formats.push_back (new Format (libdcp::Size (1436, 1080), "133", _("4:3")));
	_formats.push_back (new Format (libdcp::Size (1485, 1080), "138", _("1.375")));
	_formats.push_back (new Format (libdcp::Size (1480, 1080), "137", _("Academy")));
	_formats.push_back (new Format (libdcp::Size (1793, 1080), "166", _("1.66")));
	_formats.push_back (new Format (libdcp::Size (1920, 1080), "178", _("16:9")));
	_formats.push_back (new Format (libdcp::Size (1998, 1080), "185", _("Flat")));
	_formats.push_back (new Format (libdcp::Size (2048, 858), "239", _("Scope")));
	_formats.push_back (new Format (libdcp::Size (2048, 1080), "full-frame", _("Full frame")));
}

/** @param n Nickname.
 *  @return Matching Format, or 0.
 */
Format const *
Format::from_nickname (string n)
{
	vector<Format const *>::iterator i = _formats.begin ();
	while (i != _formats.end() && (*i)->nickname() != n) {
		++i;
	}

	if (i == _formats.end ()) {
		return 0;
	}

	return *i;
}

/** @param i Id.
 *  @return Matching Format, or 0.
 */
Format const *
Format::from_id (string i)
{
	vector<Format const *>::iterator j = _formats.begin ();
	while (j != _formats.end() && (*j)->id() != i) {
		++j;
	}

	if (j == _formats.end ()) {
		return 0;
	}

	return *j;
}

/** @return All available formats */
vector<Format const *>
Format::all ()
{
	return _formats;
}

float
Format::ratio () const
{
	return static_cast<float> (_dcp_size.width) / _dcp_size.height;
}
