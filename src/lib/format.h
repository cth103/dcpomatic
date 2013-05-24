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

/** @file src/format.h
 *  @brief Classes to describe a format (aspect ratio) that a Film should
 *  be shown in.
 */

#include <string>
#include <vector>
#include <libdcp/util.h>

class Film;

class Format
{
public:
	Format (libdcp::Size dcp, std::string id, std::string n)
		: _dcp_size (dcp)
		, _id (id)
		, _nickname (n)
	{}

	/** @return size in pixels of the images that we should
	 *  put in a DCP for this format.
	 */
	libdcp::Size dcp_size () const {
		return _dcp_size;
	}

	std::string id () const {
		return _id;
	}

	/** @return Full name to present to the user */
	std::string name () const;

	/** @return Nickname (e.g. Flat, Scope) */
	std::string nickname () const {
		return _nickname;
	}

	static Format const * from_nickname (std::string n);
	static Format const * from_id (std::string i);
	static std::vector<Format const *> all ();
	static void setup_formats ();

protected:	
        /** @return the ratio */
	float ratio () const;

	/** libdcp::Size in pixels of the images that we should
	 *  put in a DCP for this format.
	 */
	libdcp::Size _dcp_size;
	/** id for use in metadata */
	std::string _id;
	/** nickname (e.g. Flat, Scope) */
	std::string _nickname;

private:	
	/** all available formats */
	static std::vector<Format const *> _formats;
};
