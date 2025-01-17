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


#include <dcp/certificate.h>
#include <dcp/warnings.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/optional.hpp>
#include <string>


class KDMRecipient
{
public:
	KDMRecipient (std::string const& name_, std::string const& notes_, boost::optional<dcp::Certificate> recipient, boost::optional<std::string> recipient_file_)
		: name (name_)
		, notes (notes_)
		, recipient_file (recipient_file_)
		, _recipient(recipient)
	{}

	KDMRecipient(std::string const& name_, std::string const& notes_, boost::optional<std::string> recipient, boost::optional<std::string> recipient_file_)
		: name(name_)
		, notes(notes_)
		, recipient_file(recipient_file_)
		, _recipient_string(recipient)
	{}

	explicit KDMRecipient (cxml::ConstNodePtr);

	virtual ~KDMRecipient () {}

	virtual void as_xml (xmlpp::Element *) const;

	boost::optional<dcp::Certificate> recipient() const;
	void set_recipient(boost::optional<dcp::Certificate> certificate);

	std::string name;
	std::string notes;
	/** The pathname or URL that the recipient certificate was obtained from; purely
	 *  to inform the user.
	 */
	boost::optional<std::string> recipient_file;

private:
	/* The recipient certificate may be stored as either a string or a dcp::Certificate;
	 * the string is useful if we want to be lazy about constructing the dcp::Certificate.
	 */
	boost::optional<dcp::Certificate> _recipient;
	boost::optional<std::string> _recipient_string;
};


#endif
