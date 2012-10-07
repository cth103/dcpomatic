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

using namespace std;

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
	_formats.push_back (new FixedFormat (119, Size (1285, 1080), "119", "1.19"));
	_formats.push_back (new FixedFormat (133, Size (1436, 1080), "133", "1.33"));
	_formats.push_back (new FixedFormat (138, Size (1485, 1080), "138", "1.375"));
	_formats.push_back (new FixedFormat (133, Size (1998, 1080), "133-in-flat", "4:3 within Flat"));
	_formats.push_back (new FixedFormat (137, Size (1480, 1080), "137", "Academy"));
	_formats.push_back (new FixedFormat (166, Size (1793, 1080), "166", "1.66"));
	_formats.push_back (new FixedFormat (166, Size (1998, 1080), "166-in-flat", "1.66 within Flat"));
	_formats.push_back (new FixedFormat (178, Size (1998, 1080), "178-in-flat", "16:9 within Flat"));
	_formats.push_back (new FixedFormat (185, Size (1998, 1080), "185", "Flat"));
	_formats.push_back (new FixedFormat (239, Size (2048, 858), "239", "Scope"));
	_formats.push_back (new VariableFormat (Size (1998, 1080), "var-185", "Flat"));
	_formats.push_back (new VariableFormat (Size (2048, 858), "var-239", "Scope"));
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

/** @param f A Format.
 *  @return Index of f within our static list, or -1.
 */
int
Format::as_index (Format const * f)
{
	vector<Format*>::size_type i = 0;
	while (i < _formats.size() && _formats[i] != f) {
		++i;
	}

	if (i == _formats.size ()) {
		return -1;
	}

	return i;
}

/** @param i An index returned from as_index().
 *  @return Corresponding Format.
 */
Format const *
Format::from_index (int i)
{
	assert (i >= 0 && i < int(_formats.size ()));
	return _formats[i];
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
FixedFormat::FixedFormat (int r, Size dcp, string id, string n)
	: Format (dcp, id, n)
	, _ratio (r)
{

}

int
Format::dcp_padding (Film const * f) const
{
	int p = rint ((_dcp_size.width - (_dcp_size.height * ratio_as_integer(f) / 100.0)) / 2.0);

	/* This comes out -ve for Scope; bodge it */
	if (p < 0) {
		p = 0;
	}
	
	return p;
}

VariableFormat::VariableFormat (Size dcp, string id, string n)
	: Format (dcp, id, n)
{

}

int
VariableFormat::ratio_as_integer (Film const * f) const
{
	return rint (ratio_as_float (f) * 100);
}

float
VariableFormat::ratio_as_float (Film const * f) const
{
	return float (f->size().width) / f->size().height;
}

/** @return A name to be presented to the user */
string
VariableFormat::name () const
{
	stringstream s;
	if (!_nickname.empty ()) {
		s << _nickname << " (";
	}

	s << "without stretching";

	if (!_nickname.empty ()) {
		s << ")";
	}

	return s.str ();
}
