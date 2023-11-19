/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_KDM_UTIL_H
#define DCPOMATIC_KDM_UTIL_H


#include <dcp/local_time.h>
#include <utility>


namespace dcp {
	class Certificate;
}


enum class KDMCertificateOverlap {
	KDM_WITHIN_CERTIFICATE,
	KDM_OVERLAPS_CERTIFICATE,
	KDM_OUTSIDE_CERTIFICATE
};


struct KDMCertificatePeriod
{
	KDMCertificatePeriod(std::string cinema_name_, std::string screen_name_, dcp::LocalTime from_, dcp::LocalTime to_)
		: cinema_name(std::move(cinema_name_))
		, screen_name(std::move(screen_name_))
		, from(std::move(from_))
		, to(std::move(to_))
	{}

	std::string cinema_name;
	std::string screen_name;
	KDMCertificateOverlap overlap = KDMCertificateOverlap::KDM_WITHIN_CERTIFICATE;
	dcp::LocalTime from;
	dcp::LocalTime to;
};


/** @param recipient Some KDM recipient certificate.
 *  @param kdm_from Proposed KDM start time.
 *  @param kdm_to Proposed KDM end time.
 *  @return Relationship between certificate and KDM validity periods.
 */

KDMCertificatePeriod check_kdm_and_certificate_validity_periods(
	std::string const& cinema_name,
	std::string const& screen_name,
	dcp::Certificate const& recipient,
	dcp::LocalTime kdm_from,
	dcp::LocalTime kdm_to
	);


#endif

