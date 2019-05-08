/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "screen.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using std::string;
using std::vector;
using namespace dcpomatic;

Screen::Screen (cxml::ConstNodePtr node)
	: name (node->string_child("Name"))
	, notes (node->optional_string_child("Notes").get_value_or (""))
{
	if (node->optional_string_child ("Certificate")) {
		recipient = dcp::Certificate (node->string_child ("Certificate"));
	} else if (node->optional_string_child ("Recipient")) {
		recipient = dcp::Certificate (node->string_child ("Recipient"));
	}

	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children ("TrustedDevice")) {
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
	parent->add_child("Name")->add_child_text (name);
	if (recipient) {
		parent->add_child("Recipient")->add_child_text (recipient->certificate (true));
	}

	parent->add_child("Notes")->add_child_text (notes);

	BOOST_FOREACH (TrustedDevice i, trusted_devices) {
		parent->add_child("TrustedDevice")->add_child_text(i.as_string());
	}
}

vector<string>
Screen::trusted_device_thumbprints () const
{
	vector<string> t;
	BOOST_FOREACH (TrustedDevice i, trusted_devices) {
		t.push_back (i.thumbprint());
	}
	return t;
}

TrustedDevice::TrustedDevice (string thumbprint)
	: _thumbprint (thumbprint)
{

}

TrustedDevice::TrustedDevice (dcp::Certificate certificate)
	: _certificate (certificate)
{

}

string
TrustedDevice::as_string () const
{
	if (_certificate) {
		return _certificate->certificate(true);
	}

	return *_thumbprint;
}

string
TrustedDevice::thumbprint () const
{
	if (_certificate) {
		return _certificate->thumbprint ();
	}

	return *_thumbprint;
}
