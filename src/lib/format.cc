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

using std::string;
using std::setprecision;
using std::stringstream;
using std::vector;
using boost::shared_ptr;

vector<Format const *> Format::_formats;

/** @return A name to be presented to the user */
string
FixedFormat::name () const
{
	stringstream s;
	if (!_nickname.empty ()) {
		s << _nickname << " (";
	}

	s << setprecision(3) << (_ratio / 100.0) << ":1";

	if (!_nickname.empty ()) {
		s << ")";
	}

	return s.str ();
}

/** @return Identifier for this format as metadata for a Film's metadata file */
string
Format::as_metadata () const
{
	return _id;
}

/** Fill our _formats vector with all available formats */
void
Format::setup_formats ()
{
	_formats.push_back (new FixedFormat (119, libdcp::Size (1285, 1080), "119", "1.19", "F"));
	_formats.push_back (new FixedFormat (133, libdcp::Size (1436, 1080), "133", "1.33", "F"));
	_formats.push_back (new FixedFormat (138, libdcp::Size (1485, 1080), "138", "1.375", "F"));
	_formats.push_back (new FixedFormat (133, libdcp::Size (1998, 1080), "133-in-flat", "4:3 within Flat", "F"));
	_formats.push_back (new FixedFormat (137, libdcp::Size (1480, 1080), "137", "Academy", "F"));
	_formats.push_back (new FixedFormat (166, libdcp::Size (1793, 1080), "166", "1.66", "F"));
	_formats.push_back (new FixedFormat (166, libdcp::Size (1998, 1080), "166-in-flat", "1.66 within Flat", "F"));
	_formats.push_back (new FixedFormat (178, libdcp::Size (1998, 1080), "178-in-flat", "16:9 within Flat", "F"));
	_formats.push_back (new FixedFormat (178, libdcp::Size (1920, 1080), "178", "16:9", "F"));
	_formats.push_back (new FixedFormat (185, libdcp::Size (1998, 1080), "185", "Flat", "F"));
	_formats.push_back (new FixedFormat (239, libdcp::Size (2048, 858), "239", "Scope", "S"));
	_formats.push_back (new VariableFormat (libdcp::Size (1998, 1080), "var-185", "Flat", "F"));
	_formats.push_back (new VariableFormat (libdcp::Size (2048, 858), "var-239", "Scope", "S"));
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


/** @param m Metadata, as returned from as_metadata().
 *  @return Matching Format, or 0.
 */
Format const *
Format::from_metadata (string m)
{
	return from_id (m);
}

/** @return All available formats */
vector<Format const *>
Format::all ()
{
	return _formats;
}

/** @param r Ratio multiplied by 100 (e.g. 185)
 *  @param dcp Size (in pixels) of the images that we should put in a DCP.
 *  @param id ID (e.g. 185)
 *  @param n Nick name (e.g. Flat)
 */
FixedFormat::FixedFormat (int r, libdcp::Size dcp, string id, string n, string d)
	: Format (dcp, id, n, d)
	, _ratio (r)
{

}

/** @return Number of pixels (int the DCP image) to pad either side of the film
 *  (so there are dcp_padding() pixels on the left and dcp_padding() on the right)
 */
int
Format::dcp_padding (shared_ptr<const Film> f) const
{
	int p = rint ((_dcp_size.width - (_dcp_size.height * ratio_as_integer(f) / 100.0)) / 2.0);

	/* This comes out -ve for Scope; bodge it */
	if (p < 0) {
		p = 0;
	}
	
	return p;
}

float
Format::container_ratio_as_float () const
{
	return static_cast<float> (_dcp_size.width) / _dcp_size.height;
}

VariableFormat::VariableFormat (libdcp::Size dcp, string id, string n, string d)
	: Format (dcp, id, n, d)
{

}

int
VariableFormat::ratio_as_integer (shared_ptr<const Film> f) const
{
	return rint (ratio_as_float (f) * 100);
}

float
VariableFormat::ratio_as_float (shared_ptr<const Film> f) const
{
	return float (f->size().width) / f->size().height;
}

/** @return A name to be presented to the user */
string
VariableFormat::name () const
{
	return _nickname;
}
