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


#include "kdm_recipient.h"


KDMRecipient::KDMRecipient (cxml::ConstNodePtr node)
	: name (node->string_child("Name"))
	, notes (node->optional_string_child("Notes").get_value_or(""))
{
	if (node->optional_string_child("Certificate")) {
		recipient = dcp::Certificate (node->string_child("Certificate"));
	} else if (node->optional_string_child("Recipient")) {
		recipient = dcp::Certificate (node->string_child("Recipient"));
	}

	recipient_file = node->optional_string_child("RecipientFile");
}


void
KDMRecipient::as_xml (xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "Name", name);
	if (recipient) {
		cxml::add_text_child(parent, "Recipient", recipient->certificate(true));
	}
	if (recipient_file) {
		cxml::add_text_child(parent, "RecipientFile", *recipient_file);
	}

	cxml::add_text_child(parent, "Notes", notes);
}

