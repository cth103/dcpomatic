/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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
#include "kdm_with_metadata.h"
#include "film.h"
#include <dcp/raw_convert.h>
#include <boost/foreach.hpp>


using std::string;
using std::vector;
using boost::shared_ptr;
using dcp::raw_convert;


DKDMRecipient::DKDMRecipient (cxml::ConstNodePtr node)
	: KDMRecipient (node)
{
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("Email")) {
		emails.push_back (i->content());
	}

	utc_offset_hour = node->number_child<int>("UTCOffsetHour");
	utc_offset_minute = node->number_child<int>("UTCOffsetMinute");
}


void
DKDMRecipient::as_xml (xmlpp::Element* node) const
{
	KDMRecipient::as_xml (node);

	BOOST_FOREACH (string i, emails) {
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
		return KDMWithMetadataPtr();
	}

	dcp::LocalTime const begin(valid_from, recipient->utc_offset_hour, recipient->utc_offset_minute);
	dcp::LocalTime const end  (valid_to,   recipient->utc_offset_hour, recipient->utc_offset_minute);

	dcp::EncryptedKDM const kdm = film->make_kdm (
			recipient->recipient.get(),
			vector<string>(),
			cpl,
			begin,
			end,
			dcp::MODIFIED_TRANSITIONAL_1,
			true,
			0
			);

	dcp::NameFormat::Map name_values;
	name_values['f'] = film->name();
	name_values['b'] = begin.date() + " " + begin.time_of_day(true, false);
	name_values['e'] = end.date() + " " + end.time_of_day(true, false);
	name_values['i'] = kdm.cpl_id();

	return KDMWithMetadataPtr(new DCPKDMWithMetadata(name_values, 0, recipient->emails, kdm));
}

