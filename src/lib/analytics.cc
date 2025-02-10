/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "analytics.h"
#include "exceptions.h"
#include "variant.h"
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "i18n.h"


using std::string;
using boost::algorithm::trim;


Analytics* Analytics::_instance;
int const Analytics::_current_version = 1;


void
Analytics::successful_dcp_encode ()
{
	++_successful_dcp_encodes;
	write ();

	if (_successful_dcp_encodes == 20) {
		emit (
			boost::bind(
				boost::ref(Message),
				_("Congratulations!"),
				fmt::format(_(
					"<h2>You have made {} DCPs with {}!</h2>"
					"<img width=\"20%%\" src=\"memory:me.jpg\" align=\"center\">"
					"<font size=\"+1\">"
                                        "<p>Hello. I'm Carl and I'm the "
					"developer of {}. I work on it in my spare time (with the help "
					"of a volunteer team of testers and translators) and I release it "
					"as free software."

					"<p>If you find {} useful, please consider a donation to the "
					"project. Financial support will help me to spend more "
					"time developing {} and making it better!"

					"<p><ul>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=40\">Go to Paypal to donate €40</a>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=20\">Go to Paypal to donate €20</a>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=10\">Go to Paypal to donate €10</a>"
					"</ul>"

					"<p>Thank you!"
					"</font>"),
					_successful_dcp_encodes,
					variant::dcpomatic(),
					variant::dcpomatic(),
					variant::dcpomatic(),
					variant::dcpomatic()
					)
				)
			);
	}
}


void
Analytics::write () const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Analytics");

	cxml::add_text_child(root, "Version", fmt::to_string(_current_version));
	cxml::add_text_child(root, "SuccessfulDCPEncodes", fmt::to_string(_successful_dcp_encodes));

	try {
		doc.write_to_file_formatted(write_path("analytics.xml").string());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, write_path("analytics.xml"));
	}
}


void
Analytics::read ()
try
{
	cxml::Document f ("Analytics");
	f.read_file(dcp::filesystem::fix_long_path(read_path("analytics.xml")));
	_successful_dcp_encodes = f.number_child<int>("SuccessfulDCPEncodes");
} catch (...) {
	/* Never mind */
}


Analytics*
Analytics::instance ()
{
	if (!_instance) {
		_instance = new Analytics();
		_instance->read();
	}

	return _instance;
}
