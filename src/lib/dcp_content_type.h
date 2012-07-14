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

/** @file src/content_type.h
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */

#include <string>
#include <vector>

/** @class DCPContentType
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */
class DCPContentType
{
public:
	DCPContentType (std::string, std::string);

	/** @return user-visible `pretty' name */
	std::string pretty_name () const {
		return _pretty_name;
	}

	/** @return name as understood by OpenDCP */
	std::string opendcp_name () const {
		return _opendcp_name;
	}

	static DCPContentType const * from_pretty_name (std::string);
	static DCPContentType const * from_index (int);
	static int as_index (DCPContentType const *);
	static std::vector<DCPContentType const *> all ();
	static void setup_dcp_content_types ();

private:
	std::string _pretty_name;
	std::string _opendcp_name;

	/** All available DCP content types */
	static std::vector<DCPContentType const *> _dcp_content_types;
};
     
