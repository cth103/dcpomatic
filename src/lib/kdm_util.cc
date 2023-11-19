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


#include "kdm_util.h"
#include "screen.h"
#include <dcp/certificate.h>
#include <boost/optional.hpp>

#include "i18n.h"


using std::list;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;


KDMCertificatePeriod
check_kdm_and_certificate_validity_periods(
	string const& cinema_name,
	string const& screen_name,
	dcp::Certificate const& recipient,
	dcp::LocalTime kdm_from,
	dcp::LocalTime kdm_to
	)
{
	auto overlaps = [](dcp::LocalTime from_a, dcp::LocalTime to_a, dcp::LocalTime from_b, dcp::LocalTime to_b) {
		return std::max(from_a, from_b) < std::min(to_a, to_b);
	};

	auto contains = [](dcp::LocalTime bigger_from, dcp::LocalTime bigger_to, dcp::LocalTime smaller_from, dcp::LocalTime smaller_to) {
		return bigger_from <= smaller_from && bigger_to >= smaller_to;
	};

	KDMCertificatePeriod period(
		cinema_name,
		screen_name,
		recipient.not_before(),
		recipient.not_after()
		);

	if (contains(recipient.not_before(), recipient.not_after(), kdm_from, kdm_to)) {
		period.overlap = KDMCertificateOverlap::KDM_WITHIN_CERTIFICATE;
		return period;
	}

	if (overlaps(recipient.not_before(), recipient.not_after(), kdm_from, kdm_to)) {
		/* The KDM overlaps the certificate validity: maybe not the end of the world */
		period.overlap = KDMCertificateOverlap::KDM_OVERLAPS_CERTIFICATE;
		return period;
	} else {
		/* The KDM validity is totally outside the certificate validity: bad news */
		period.overlap = KDMCertificateOverlap::KDM_OUTSIDE_CERTIFICATE;
		return period;
	}
}

