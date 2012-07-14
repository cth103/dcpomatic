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
 *  @brief Class to describe a format (aspect ratio) that a Film should
 *  be shown in.
 */

#include <string>
#include <vector>
#include "util.h"

/** @class Format
 *  @brief Class to describe a format (aspect ratio) that a Film should
 *  be shown in.
 */
class Format
{
public:
	Format (int, Size, std::string, std::string);

	/** @return the aspect ratio multiplied by 100
	 *  (e.g. 239 for Cinemascope 2.39:1)
	 */
	int ratio_as_integer () const {
		return _ratio;
	}

	/** @return the ratio as a floating point number */
	float ratio_as_float () const {
		return _ratio / 100.0;
	}

	/** @return size in pixels of the images that we should
	 *  put in a DCP for this ratio.  This size will not correspond
	 *  to the ratio when we are doing things like 16:9 in a Flat frame.
	 */
	Size dcp_size () const {
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

	std::string as_metadata () const;

	int dcp_padding () const;

	static Format const * from_ratio (int);
	static Format const * from_nickname (std::string n);
	static Format const * from_metadata (std::string m);
	static Format const * from_index (int i);
	static Format const * from_id (std::string i);
	static int as_index (Format const * f);
	static std::vector<Format const *> all ();
	static void setup_formats ();
	
private:

	/** Ratio expressed as the actual ratio multiplied by 100 */
	int _ratio;
	/** Size in pixels of the images that we should
	 *  put in a DCP for this ratio.  This size will not correspond
	 *  to the ratio when we are doing things like 16:9 in a Flat frame.
	 */
	Size _dcp_size;
	/** id for use in metadata */
	std::string _id;
	/** nickname (e.g. Flat, Scope) */
	std::string _nickname;

	/** all available formats */
	static std::vector<Format const *> _formats;
};

	
