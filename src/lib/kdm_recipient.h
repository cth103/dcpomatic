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


#ifndef DCPOMATIC_KDM_RECIPIENT_H
#define DCPOMATIC_KDM_RECIPIENT_H


#include "warnings.h"
#include <dcp/certificate.h>
#include <libcxml/cxml.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/optional.hpp>
#include <string>


class KDMRecipient
{
public:
	KDMRecipient (std::string const& name_, std::string const& notes_, boost::optional<dcp::Certificate> recipient_)
		: name (name_)
		, notes (notes_)
		, recipient (recipient_)
	{}

	explicit KDMRecipient (cxml::ConstNodePtr);

	virtual ~KDMRecipient () {}

	virtual void as_xml (xmlpp::Element *) const;

	std::string name;
	std::string notes;
	boost::optional<dcp::Certificate> recipient;
};


#endif
