/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/analytics.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>


using std::string;


BOOST_AUTO_TEST_CASE(many_successful_encodes_test)
{
	dcp::filesystem::remove_all(*State::override_path);

	Analytics analytics;

	string last_title;
	string last_body;
	analytics.Message.connect([&](string title, string body) {
		last_title = title;
		last_body = body;
	});

	for (int i = 0; i < 19; ++i) {
		analytics.successful_dcp_encode();
		BOOST_CHECK(last_title.empty());
		BOOST_CHECK(last_body.empty());
	}

	analytics.successful_dcp_encode();
	BOOST_CHECK_EQUAL(last_title, "Congratulations!");
	BOOST_CHECK_EQUAL(last_body,
		"<h2>You have made 20 DCPs with DCP-o-matic!</h2>"
		"<img width=\"20%%\" src=\"memory:me.jpg\" align=\"center\">"
		"<font size=\"+1\">"
                "<p>Hello. I'm Carl and I'm the "
		"developer of DCP-o-matic. I work on it in my spare time (with the help "
		"of a volunteer team of testers and translators) and I release it "
		"as free software."

		"<p>If you find DCP-o-matic useful, please consider a donation to the "
		"project. Financial support will help me to spend more "
		"time developing DCP-o-matic and making it better!"

		"<p><ul>"
		"<li><a href=\"https://dcpomatic.com/donate_amount?amount=40\">Go to Paypal to donate €40</a>"
		"<li><a href=\"https://dcpomatic.com/donate_amount?amount=20\">Go to Paypal to donate €20</a>"
		"<li><a href=\"https://dcpomatic.com/donate_amount?amount=10\">Go to Paypal to donate €10</a>"
		"</ul>"

		"<p>Thank you!"
		"</font>"
	);
}

