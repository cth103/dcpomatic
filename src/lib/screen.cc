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


#include "cinema.h"
#include "cinema_list.h"
#include "config.h"
#include "film.h"
#include "kdm_util.h"
#include "kdm_with_metadata.h"
#include "screen.h"
#include <libxml++/libxml++.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


vector<string>
Screen::trusted_device_thumbprints () const
{
	vector<string> t;
	for (auto i: trusted_devices) {
		t.push_back (i.thumbprint());
	}
	return t;
}


KDMWithMetadataPtr
kdm_for_screen (
	std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm,
	CinemaID cinema_id,
	Cinema const& cinema,
	Screen const& screen,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio,
	vector<KDMCertificatePeriod>& period_checks
	)
{
	if (!screen.recipient()) {
		return {};
	}

	dcp::LocalTime const begin(valid_from, cinema.utc_offset);
	dcp::LocalTime const end  (valid_to,   cinema.utc_offset);

	period_checks.push_back(check_kdm_and_certificate_validity_periods(cinema.name, screen.name, screen.recipient().get(), begin, end));

	auto signer = Config::instance()->signer_chain();
	if (!signer->valid()) {
		throw InvalidSignerError();
	}

	auto kdm = make_kdm(begin, end).encrypt(
		signer, screen.recipient().get(), screen.trusted_device_thumbprints(), formulation, disable_forensic_marking_picture, disable_forensic_marking_audio
		);

	dcp::NameFormat::Map name_values;
	name_values['c'] = cinema.name;
	name_values['s'] = screen.name;
	name_values['f'] = kdm.content_title_text();
	name_values['b'] = begin.date() + " " + begin.time_of_day(true, false);
	name_values['e'] = end.date() + " " + end.time_of_day(true, false);
	name_values['i'] = kdm.cpl_id();

	return make_shared<KDMWithMetadata>(name_values, cinema_id, cinema.emails, kdm);
}

