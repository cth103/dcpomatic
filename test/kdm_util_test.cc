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


#include "lib/kdm_util.h"
#include <dcp/certificate.h>
#include <dcp/util.h>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(check_kdm_and_certificate_validity_periods_good)
{
	auto const result = check_kdm_and_certificate_validity_periods(
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		dcp::LocalTime("2023-01-03T10:30:00"),
		dcp::LocalTime("2050-10-20T14:00:00")
		);

	BOOST_CHECK(result == KDMCertificatePeriod::KDM_WITHIN_CERTIFICATE);
}


BOOST_AUTO_TEST_CASE(check_kdm_and_certificate_validity_periods_overlap_start)
{
	auto const result = check_kdm_and_certificate_validity_periods(
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		dcp::LocalTime("2011-01-03T10:30:00"),
		dcp::LocalTime("2050-10-20T14:00:00")
		);

	BOOST_CHECK(result == KDMCertificatePeriod::KDM_OVERLAPS_CERTIFICATE);
}


BOOST_AUTO_TEST_CASE(check_kdm_and_certificate_validity_periods_overlap_end)
{
	auto const result = check_kdm_and_certificate_validity_periods(
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		dcp::LocalTime("2033-01-03T10:30:00"),
		dcp::LocalTime("2095-10-20T14:00:00")
		);

	BOOST_CHECK(result == KDMCertificatePeriod::KDM_OVERLAPS_CERTIFICATE);
}


BOOST_AUTO_TEST_CASE(check_kdm_and_certificate_validity_periods_overlap_start_and_end)
{
	auto const result = check_kdm_and_certificate_validity_periods(
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		dcp::LocalTime("2011-01-03T10:30:00"),
		dcp::LocalTime("2095-10-20T14:00:00")
		);

	BOOST_CHECK(result == KDMCertificatePeriod::KDM_OVERLAPS_CERTIFICATE);
}


BOOST_AUTO_TEST_CASE(check_kdm_and_certificate_validity_periods_outside)
{
	auto const result = check_kdm_and_certificate_validity_periods(
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		dcp::LocalTime("2011-01-03T10:30:00"),
		dcp::LocalTime("2012-10-20T14:00:00")
		);

	BOOST_CHECK(result == KDMCertificatePeriod::KDM_OUTSIDE_CERTIFICATE);
}
