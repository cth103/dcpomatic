/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/cinema.h
 *  @brief Cinema class.
 */


#include <dcp/utc_offset.h>
#include <memory>
#include <string>
#include <vector>


/** @class Cinema
 *  @brief A description of a Cinema for KDM generation.
 *
 *  This is a cinema name and some metadata.
 */
class Cinema
{
public:
	Cinema(std::string const & name_, std::vector<std::string> const & e, std::string notes_, dcp::UTCOffset utc_offset_)
		: name (name_)
		, emails (e)
		, notes (notes_)
		, utc_offset(std::move(utc_offset_))
	{}

	std::string name;
	std::vector<std::string> emails;
	std::string notes;
	dcp::UTCOffset utc_offset;
};
