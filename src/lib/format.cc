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
FixedFormat::name () const
{
	stringstream s;
	if (!_nickname.empty ()) {
		s << _nickname << N_(" (");
	}

	s << setprecision(3) << _ratio << N_(":1");

	if (!_nickname.empty ()) {
		s << N_(")");
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
	/// TRANSLATORS: these are film picture aspect ratios; "Academy" means 1.37, "Flat" 1.85 and "Scope" 2.39.
	_formats.push_back (
		new FixedFormat (1.19, libdcp::Size (1285, 1080), "119", _("1.19"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (4.0 / 3.0, libdcp::Size (1436, 1080), "133", _("4:3"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.38, libdcp::Size (1485, 1080), "138", _("1.375"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (4.0 / 3.0, libdcp::Size (1998, 1080), "133-in-flat", _("4:3 within Flat"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.37, libdcp::Size (1480, 1080), "137", _("Academy"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.66, libdcp::Size (1793, 1080), "166", _("1.66"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.66, libdcp::Size (1998, 1080), "166-in-flat", _("1.66 within Flat"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.78, libdcp::Size (1998, 1080), "178-in-flat", _("16:9 within Flat"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.78, libdcp::Size (1920, 1080), "178", _("16:9"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.85, libdcp::Size (1998, 1080), "185", _("Flat"), "F"
			));
	
	_formats.push_back (
		new FixedFormat (1.78, libdcp::Size (2048, 858), "178-in-scope", _("16:9 within Scope"), "S"
			));
	
	_formats.push_back (
		new FixedFormat (2.39, libdcp::Size (2048, 858), "239", _("Scope"), "S"
			));

	_formats.push_back (
		new FixedFormat (1.896, libdcp::Size (2048, 1080), "full-frame", _("Full frame"), "C"
			));
		
	_formats.push_back (
		new VariableFormat (libdcp::Size (1998, 1080), "var-185", _("Flat without stretch"), "F"
			));
	
	_formats.push_back (
		new VariableFormat (libdcp::Size (2048, 858), "var-239", _("Scope without stretch"), "S"
			));
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

/** @param r Ratio
 *  @param dcp Size (in pixels) of the images that we should put in a DCP.
 *  @param id ID (e.g. 185)
 *  @param n Nick name (e.g. Flat)
 */
FixedFormat::FixedFormat (float r, libdcp::Size dcp, string id, string n, string d)
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
	int p = rint ((_dcp_size.width - (_dcp_size.height * ratio(f))) / 2.0);

	/* This comes out -ve for Scope; bodge it */
	if (p < 0) {
		p = 0;
	}
	
	return p;
}

float
Format::container_ratio () const
{
	return static_cast<float> (_dcp_size.width) / _dcp_size.height;
}

VariableFormat::VariableFormat (libdcp::Size dcp, string id, string n, string d)
	: Format (dcp, id, n, d)
{

}

float
VariableFormat::ratio (shared_ptr<const Film> f) const
{
	libdcp::Size const c = f->cropped_size (f->size ());
	return float (c.width) / c.height;
}

/** @return A name to be presented to the user */
string
VariableFormat::name () const
{
	return _nickname;
}
