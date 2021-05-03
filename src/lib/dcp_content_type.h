/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef DCPOMATIC_DCP_CONTENT_TYPE_H
#define DCPOMATIC_DCP_CONTENT_TYPE_H


/** @file src/dcp_content_type.h
 *  @brief DCPContentType class.
 */


#include <dcp/dcp.h>
#include <string>
#include <vector>


/** @class DCPContentType
 *  @brief A description of the type of content for a DCP (e.g. feature, trailer etc.)
 */
class DCPContentType
{
public:
	DCPContentType (std::string, dcp::ContentKind, std::string);

	DCPContentType (DCPContentType const&) = delete;
	DCPContentType& operator= (DCPContentType const&) = delete;

	/** @return user-visible `pretty' name */
	std::string pretty_name () const {
		return _pretty_name;
	}

	dcp::ContentKind libdcp_kind () const {
		return _libdcp_kind;
	}

	std::string isdcf_name () const {
		return _isdcf_name;
	}

	static DCPContentType const * from_isdcf_name (std::string);
	static DCPContentType const * from_libdcp_kind (dcp::ContentKind);
	static DCPContentType const * from_index (int);
	static boost::optional<int> as_index (DCPContentType const *);
	static std::vector<DCPContentType const *> all ();
	static void setup_dcp_content_types ();

private:
	std::string _pretty_name;
	dcp::ContentKind _libdcp_kind;
	std::string _isdcf_name;

	/** All available DCP content types */
	static std::vector<DCPContentType const *> _dcp_content_types;
};


#endif
