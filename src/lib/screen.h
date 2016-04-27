/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include <dcp/certificate.h>
#include <libcxml/cxml.h>
#include <boost/optional.hpp>
#include <string>

class Cinema;

/** @class Screen
 *  @brief A representation of a Screen for KDM generation.
 *
 *  This is the name of the screen and the certificate of its
 *  `recipient' (i.e. the servers).
 */
class Screen
{
public:
	Screen (std::string const & n, boost::optional<dcp::Certificate> rec, std::vector<dcp::Certificate> td)
		: name (n)
		, recipient (rec)
		, trusted_devices (td)
	{}

	Screen (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const;

	boost::shared_ptr<Cinema> cinema;
	std::string name;
	std::string notes;
	boost::optional<dcp::Certificate> recipient;
	std::vector<dcp::Certificate> trusted_devices;
};
