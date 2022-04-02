/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "dkdm_recipient.h"
#include "film.h"
#include "kdm_with_metadata.h"
#include <dcp/raw_convert.h>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using dcp::raw_convert;


DKDMRecipient::DKDMRecipient (cxml::ConstNodePtr node)
	: KDMRecipient (node)
{
	for (auto i: node->node_children("Email")) {
		emails.push_back (i->content());
	}

	utc_offset_hour = node->number_child<int>("UTCOffsetHour");
	utc_offset_minute = node->number_child<int>("UTCOffsetMinute");
}


void
DKDMRecipient::as_xml (xmlpp::Element* node) const
{
	KDMRecipient::as_xml (node);

	for (auto i: emails) {
		node->add_child("Email")->add_child_text(i);
	}

	node->add_child("UTCOffsetHour")->add_child_text(raw_convert<string>(utc_offset_hour));
	node->add_child("UTCOffsetMinute")->add_child_text(raw_convert<string>(utc_offset_minute));
}


KDMWithMetadataPtr
kdm_for_dkdm_recipient (
	shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	shared_ptr<DKDMRecipient> recipient,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to
	)
{
	if (!recipient->recipient) {
		return {};
	}

	dcp::LocalTime const begin(valid_from, recipient->utc_offset_hour, recipient->utc_offset_minute);
	dcp::LocalTime const end  (valid_to,   recipient->utc_offset_hour, recipient->utc_offset_minute);

	auto const kdm = film->make_kdm (
			recipient->recipient.get(),
			vector<string>(),
			cpl,
			begin,
			end,
			dcp::Formulation::MODIFIED_TRANSITIONAL_1,
			true,
			0
			);

	dcp::NameFormat::Map name_values;
	name_values['f'] = kdm.content_title_text();
	name_values['b'] = begin.date() + " " + begin.time_of_day(true, false);
	name_values['e'] = end.date() + " " + end.time_of_day(true, false);
	name_values['i'] = kdm.cpl_id();

	return make_shared<KDMWithMetadata>(name_values, nullptr, recipient->emails, kdm);
}

