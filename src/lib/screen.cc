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
#include "film.h"
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


Screen::Screen (cxml::ConstNodePtr node)
	: KDMRecipient (node)
{
	for (auto i: node->node_children ("TrustedDevice")) {
		if (boost::algorithm::starts_with(i->content(), "-----BEGIN CERTIFICATE-----")) {
			trusted_devices.push_back (TrustedDevice(dcp::Certificate(i->content())));
		} else {
			trusted_devices.push_back (TrustedDevice(i->content()));
		}
	}
}


void
Screen::as_xml (xmlpp::Element* parent) const
{
	KDMRecipient::as_xml (parent);
	for (auto i: trusted_devices) {
		parent->add_child("TrustedDevice")->add_child_text(i.as_string());
	}
}


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
	shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	shared_ptr<const dcpomatic::Screen> screen,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio
	)
{
	if (!screen->recipient) {
		return {};
	}

	auto cinema = screen->cinema;
	dcp::LocalTime const begin(valid_from, cinema ? cinema->utc_offset_hour() : 0, cinema ? cinema->utc_offset_minute() : 0);
	dcp::LocalTime const end  (valid_to,   cinema ? cinema->utc_offset_hour() : 0, cinema ? cinema->utc_offset_minute() : 0);

	auto const kdm = film->make_kdm (
			screen->recipient.get(),
			screen->trusted_device_thumbprints(),
			cpl,
			begin,
			end,
			formulation,
			disable_forensic_marking_picture,
			disable_forensic_marking_audio
			);

	dcp::NameFormat::Map name_values;
	if (cinema) {
		name_values['c'] = cinema->name;
	} else {
		name_values['c'] = "";
	}
	name_values['s'] = screen->name;
	name_values['f'] = film->name();
	name_values['b'] = begin.date() + " " + begin.time_of_day(true, false);
	name_values['e'] = end.date() + " " + end.time_of_day(true, false);
	name_values['i'] = kdm.cpl_id();

	return make_shared<KDMWithMetadata>(name_values, cinema.get(), cinema ? cinema->emails : list<string>(), kdm);
}

