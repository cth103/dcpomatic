/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include "kdm_recipient.h"
#include "kdm_util.h"
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
class Screen : public KDMRecipient
{
public:
	Screen (
		std::string const & name_,
		std::string const & notes_,
		boost::optional<dcp::Certificate> recipient_,
		boost::optional<std::string> recipient_file_,
		std::vector<TrustedDevice> trusted_devices_
	       )
		: KDMRecipient (name_, notes_, recipient_, recipient_file_)
		, trusted_devices (trusted_devices_)
	{}

	explicit Screen (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const override;
	std::vector<std::string> trusted_device_thumbprints () const;

	std::shared_ptr<Cinema> cinema;
	std::vector<TrustedDevice> trusted_devices;
};

}


KDMWithMetadataPtr
kdm_for_screen (
	std::shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	std::shared_ptr<const dcpomatic::Screen> screen,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	boost::optional<int> disable_forensic_marking_audio,
	std::vector<KDMCertificatePeriod>& period_checks
	);


#endif
