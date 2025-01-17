/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/cinema.h"
#include "lib/cinema_list.h"
#include "lib/config.h"
#include "lib/screen.h"
#include "test.h"
#include <dcp/certificate.h>
#include <dcp/filesystem.h>
#include <dcp/util.h>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <list>
#include <string>


using std::pair;
using std::string;
using std::vector;


static
boost::filesystem::path
setup(string name)
{
	boost::filesystem::path db = boost::filesystem::path("build") / "test" / (name + ".db");
	boost::system::error_code ec;
	boost::filesystem::remove(db, ec);
	return db;
}


BOOST_AUTO_TEST_CASE(add_cinema_test)
{
	auto const db = setup("add_cinema_test");

	auto const name = "Bob's Zero-G Cinema";
	auto const emails = vector<string>{"zerogbob@hotmail.com"};
	auto const notes = "Nice enough place but the popcorn keeps floating away";
	auto const utc_offset = dcp::UTCOffset{5, 0};

	CinemaList cinemas(db);
	cinemas.add_cinema({name, emails, notes, utc_offset});

	CinemaList cinemas2(db);
	auto const check = cinemas2.cinemas();
	BOOST_REQUIRE_EQUAL(check.size(), 1U);
	BOOST_CHECK(check[0].second.name == name);
	BOOST_CHECK(check[0].second.emails == emails);
	BOOST_CHECK_EQUAL(check[0].second.notes, notes);
	BOOST_CHECK(check[0].second.utc_offset == utc_offset);
}


BOOST_AUTO_TEST_CASE(remove_cinema_test)
{
	auto const db = setup("remove_cinema_test");

	auto const name1 = "Bob's Zero-G Cinema";
	auto const emails1 = vector<string>{"zerogbob@hotmail.com"};
	auto const notes1 = "Nice enough place but the popcorn keeps floating away";
	auto const utc_offset1 = dcp::UTCOffset{-4, -30};

	auto const name2 = "Angie's Infinite-Screen Cinema";
	auto const emails2 = vector<string>{"angie@infinitium.com", "projection-screen912341235@infinitium.com"};
	auto const notes2 = "Nice enough place but it's very hard to find the right screen";
	auto const utc_offset2 = dcp::UTCOffset{9, 0};

	CinemaList cinemas(db);
	auto const id1 = cinemas.add_cinema({name1, emails1, notes1, utc_offset1});
	cinemas.add_cinema({name2, emails2, notes2, utc_offset2});

	auto const check = cinemas.cinemas();
	BOOST_REQUIRE_EQUAL(check.size(), 2U);
	BOOST_CHECK(check[0].second.name == name2);
	BOOST_CHECK(check[0].second.emails == emails2);
	BOOST_CHECK_EQUAL(check[0].second.notes, notes2);
	BOOST_CHECK(check[0].second.utc_offset == utc_offset2);
	BOOST_CHECK(check[1].second.name == name1);
	BOOST_CHECK(check[1].second.emails == emails1);
	BOOST_CHECK_EQUAL(check[1].second.notes, notes1);
	BOOST_CHECK(check[1].second.utc_offset == utc_offset1);

	cinemas.remove_cinema(id1);

	auto const check2 = cinemas.cinemas();
	BOOST_REQUIRE_EQUAL(check2.size(), 1U);
	BOOST_CHECK(check2[0].second.name == name2);
	BOOST_CHECK(check2[0].second.emails == emails2);
	BOOST_CHECK_EQUAL(check2[0].second.notes, notes2);
}


BOOST_AUTO_TEST_CASE(update_cinema_test)
{
	auto const db = setup("update_cinema_test");

	auto const name1 = "Bob's Zero-G Cinema";
	auto const emails1 = vector<string>{"zerogbob@hotmail.com"};
	auto const notes1 = "Nice enough place but the popcorn keeps floating away";
	auto const utc_offset1 = dcp::UTCOffset{-4, -30};

	auto const name2 = "Angie's Infinite-Screen Cinema";
	auto const emails2 = vector<string>{"angie@infinitium.com", "projection-screen912341235@infinitium.com"};
	auto const notes2 = "Nice enough place but it's very hard to find the right screen";
	auto const utc_offset2 = dcp::UTCOffset{9, 0};

	CinemaList cinemas(db);
	auto const id = cinemas.add_cinema({name1, emails1, notes1, utc_offset1});
	cinemas.add_cinema({name2, emails2, notes2, utc_offset2});

	auto check = cinemas.cinemas();
	BOOST_REQUIRE_EQUAL(check.size(), 2U);
	/* Alphabetically ordered so first is 2 */
	BOOST_CHECK_EQUAL(check[0].second.name, name2);
	BOOST_CHECK(check[0].second.emails == emails2);
	BOOST_CHECK_EQUAL(check[0].second.notes, notes2);
	BOOST_CHECK(check[0].second.utc_offset == utc_offset2);
	/* Then 1 */
	BOOST_CHECK_EQUAL(check[1].second.name, name1);
	BOOST_CHECK(check[1].second.emails == emails1);
	BOOST_CHECK_EQUAL(check[1].second.notes, notes1);
	BOOST_CHECK(check[1].second.utc_offset == utc_offset1);

	cinemas.update_cinema(id, Cinema{name1, vector<string>{"bob@zerogkino.com"}, notes1, utc_offset1});

	check = cinemas.cinemas();
	BOOST_REQUIRE_EQUAL(check.size(), 2U);
	BOOST_CHECK_EQUAL(check[0].second.name, name2);
	BOOST_CHECK(check[0].second.emails == emails2);
	BOOST_CHECK_EQUAL(check[0].second.notes, notes2);
	BOOST_CHECK(check[0].second.utc_offset == utc_offset2);
	BOOST_CHECK_EQUAL(check[1].second.name, name1);
	BOOST_CHECK(check[1].second.emails == vector<string>{"bob@zerogkino.com"});
	BOOST_CHECK_EQUAL(check[1].second.notes, notes1);
	BOOST_CHECK(check[1].second.utc_offset == utc_offset1);
}


BOOST_AUTO_TEST_CASE(add_screen_test)
{
	auto const db = setup("add_screen_test");

	CinemaList cinemas(db);
	auto const cinema_id = cinemas.add_cinema({"Name", { "foo@bar.com" }, "", dcp::UTCOffset()});
	auto const screen_id = cinemas.add_screen(
		cinema_id,
		dcpomatic::Screen(
			"Screen 1",
			"Smells of popcorn",
			dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
			string("test/data/cert.pem"),
			vector<TrustedDevice>{}
			));

	auto check = cinemas.screens(cinema_id);
	BOOST_REQUIRE_EQUAL(check.size(), 1U);
	BOOST_CHECK(check[0].first == screen_id);
	BOOST_CHECK_EQUAL(check[0].second.name, "Screen 1");
	BOOST_CHECK_EQUAL(check[0].second.notes, "Smells of popcorn");
	BOOST_CHECK(check[0].second.recipient() == dcp::Certificate(dcp::file_to_string("test/data/cert.pem")));
	BOOST_CHECK(check[0].second.recipient_file == string("test/data/cert.pem"));
}


BOOST_AUTO_TEST_CASE(update_screen_test)
{
	auto const db = setup("update_screen_test");

	CinemaList cinemas(db);
	auto const cinema_id = cinemas.add_cinema({"Name", { "foo@bar.com" }, "", dcp::UTCOffset()});

	auto screen = dcpomatic::Screen(
		"Screen 1",
		"Smells of popcorn",
		dcp::Certificate(dcp::file_to_string("test/data/cert.pem")),
		string("test/data/cert.pem"),
		vector<TrustedDevice>{}
		);

	auto const screen_id = cinemas.add_screen(cinema_id, screen);

	screen.name = "Screen 1 updated";
	screen.notes = "Smells of popcorn and hope";
	cinemas.update_screen(cinema_id, screen_id, screen);

	auto check = cinemas.screens(cinema_id);
	BOOST_REQUIRE_EQUAL(check.size(), 1U);
	BOOST_CHECK(check[0].first == screen_id);
	BOOST_CHECK_EQUAL(check[0].second.name, "Screen 1 updated");
	BOOST_CHECK_EQUAL(check[0].second.notes, "Smells of popcorn and hope");
	BOOST_CHECK(check[0].second.recipient() == dcp::Certificate(dcp::file_to_string("test/data/cert.pem")));
	BOOST_CHECK(check[0].second.recipient_file == string("test/data/cert.pem"));
}


BOOST_AUTO_TEST_CASE(cinemas_list_copy_from_xml_test)
{
	ConfigRestorer cr("build/test/cinemas_list_copy_config");

	dcp::filesystem::remove_all(*Config::override_path);
	dcp::filesystem::create_directories(*Config::override_path);
	dcp::filesystem::copy_file("test/data/cinemas2.xml", *Config::override_path / "cinemas2.xml");

	CinemaList cinema_list;
	cinema_list.read_legacy_file(Config::instance()->read_path("cinemas2.xml"));
	auto cinemas = cinema_list.cinemas();
	BOOST_REQUIRE_EQUAL(cinemas.size(), 3U);

	auto cinema_iter = cinemas.begin();
	BOOST_CHECK_EQUAL(cinema_iter->second.name, "Great");
	BOOST_CHECK_EQUAL(cinema_iter->second.emails.size(), 1U);
	BOOST_CHECK_EQUAL(cinema_iter->second.emails[0], "julie@tinyscreen.com");
	BOOST_CHECK(cinema_iter->second.utc_offset == dcp::UTCOffset(1, 0));
	++cinema_iter;

	BOOST_CHECK_EQUAL(cinema_iter->second.name, "classy joint");
	BOOST_CHECK_EQUAL(cinema_iter->second.notes, "Can't stand this place");
	++cinema_iter;

	BOOST_CHECK_EQUAL(cinema_iter->second.name, "stinking dump");
	BOOST_CHECK_EQUAL(cinema_iter->second.emails.size(), 2U);
	BOOST_CHECK_EQUAL(cinema_iter->second.emails[0], "bob@odourscreen.com");
	BOOST_CHECK_EQUAL(cinema_iter->second.emails[1], "alice@whiff.com");
	BOOST_CHECK_EQUAL(cinema_iter->second.notes, "Great cinema, smells of roses");
	BOOST_CHECK(cinema_iter->second.utc_offset == dcp::UTCOffset(-7, 0));
	auto screens = cinema_list.screens(cinema_iter->first);
	BOOST_CHECK_EQUAL(screens.size(), 2U);
	auto screen_iter = screens.begin();
	BOOST_CHECK_EQUAL(screen_iter->second.name, "1");
	BOOST_CHECK(screen_iter->second.recipient());
	BOOST_CHECK_EQUAL(screen_iter->second.recipient()->subject_dn_qualifier(), "CVsuuv9eYsQZSl8U4fDpvOmzZhI=");
	++screen_iter;
	BOOST_CHECK_EQUAL(screen_iter->second.name, "2");
	BOOST_CHECK(screen_iter->second.recipient());
	BOOST_CHECK_EQUAL(screen_iter->second.recipient()->subject_dn_qualifier(), "CVsuuv9eYsQZSl8U4fDpvOmzZhI=");
}

