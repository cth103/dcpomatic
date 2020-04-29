/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SCREEN_H
#define DCPOMATIC_SCREEN_H

#include "kdm_with_metadata.h"
#include "trusted_device.h"
#include <dcp/certificate.h>
#include <libcxml/cxml.h>
#include <boost/optional.hpp>
#include <string>

class Cinema;
class Film;

namespace dcpomatic {

/** @class Screen
 *  @brief A representation of a Screen for KDM generation.
 *
 *  This is the name of the screen, the certificate of its
 *  `recipient' (i.e. the mediablock) and the certificates/thumbprints
 *  of any trusted devices.
 */
class Screen
{
public:
	Screen (std::string const & na, std::string const & no, boost::optional<dcp::Certificate> rec, std::vector<TrustedDevice> td)
		: name (na)
		, notes (no)
		, recipient (rec)
		, trusted_devices (td)
	{}

	explicit Screen (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const;
	std::vector<std::string> trusted_device_thumbprints () const;

	boost::shared_ptr<Cinema> cinema;
	std::string name;
	std::string notes;
	boost::optional<dcp::Certificate> recipient;
	std::vector<TrustedDevice> trusted_devices;
};

}

KDMWithMetadataPtr
kdm_for_screen (
	boost::shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	boost::shared_ptr<const dcpomatic::Screen> screen,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	boost::optional<int> disable_forensic_marking_audio
	);


#endif
