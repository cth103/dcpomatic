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

#include "kdm_recipient.h"
#include "kdm_with_metadata.h"

class Film;

class DKDMRecipient : public KDMRecipient
{
public:
	DKDMRecipient (
		std::string const& name_,
		std::string const& notes_,
		boost::optional<dcp::Certificate> recipient_,
		std::list<std::string> emails_,
		int utc_offset_hour_,
		int utc_offset_minute_
		)
		: KDMRecipient (name_, notes_, recipient_)
	  	, emails (emails_)
		, utc_offset_hour (utc_offset_hour_)
		, utc_offset_minute (utc_offset_minute_)
	{

	}

	explicit DKDMRecipient (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const;

	std::list<std::string> emails;
	int utc_offset_hour;
	int utc_offset_minute;
};


KDMWithMetadataPtr
kdm_for_dkdm_recipient (
	std::shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	std::shared_ptr<DKDMRecipient> recipient,
	boost::posix_time::ptime valid_from,
	boost::posix_time::ptime valid_to
	);

