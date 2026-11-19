/*
    Copyright (C) 2026 Jaume Puig

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
#include "lib/kdm_cli.h"
#include "lib/screen.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>


using std::string;
using std::vector;
using boost::optional;


static
optional<string>
run_cli(vector<string> const& args, vector<string>& output)
{
	std::vector<char*> argv(args.size());
	for (auto i = 0U; i < args.size(); ++i) {
		argv[i] = const_cast<char*>(args[i].c_str());
	}
	return kdm_cli(args.size(), argv.data(), [&output](string s) { output.push_back(s); });
}


static
boost::filesystem::path
fresh_cinemas_file(string name)
{
	auto file = boost::filesystem::current_path() / "build" / "test" / (name + ".sqlite3");
	boost::filesystem::remove(file);
	return file;
}


BOOST_AUTO_TEST_CASE(kdm_cli_add_cinema_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_add_cinema_test");
	vector<string> output;

	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "--emails", "fred@a.com,joe@b.org", "--notes", "back door", "--utc-offset", "-5:30", "add-cinema"}, output));
	BOOST_REQUIRE_EQUAL(output.size(), 1U);

	{
		CinemaList list(file);
		auto cinemas = list.cinemas();
		BOOST_REQUIRE_EQUAL(cinemas.size(), 1U);
		BOOST_CHECK_EQUAL(cinemas[0].second.name, "Fred's Cinema");
		BOOST_CHECK_EQUAL(cinemas[0].second.notes, "back door");
		BOOST_REQUIRE_EQUAL(cinemas[0].second.emails.size(), 2U);
		BOOST_CHECK_EQUAL(cinemas[0].second.emails[1], "joe@b.org");
		BOOST_CHECK_EQUAL(cinemas[0].second.utc_offset.hour(), -5);
		BOOST_CHECK_EQUAL(cinemas[0].second.utc_offset.minute(), -30);
	}

	/* The same name again, a missing name and bad offsets are refused */
	output.clear();
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "add-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "add-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "--utc-offset", "abc", "add-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "--utc-offset", "+15", "add-cinema"}, output));
	CinemaList list(file);
	BOOST_CHECK_EQUAL(list.cinemas().size(), 1U);
}


BOOST_AUTO_TEST_CASE(kdm_cli_add_screen_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_add_screen_test");
	vector<string> output;
	auto const cert = string("test/data/cert.pem");

	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "add-cinema"}, output));
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "-C", cert, "--notes", "GDC SR1000", "add-screen"}, output));

	{
		CinemaList list(file);
		auto cinemas = list.cinemas();
		BOOST_REQUIRE_EQUAL(cinemas.size(), 1U);
		auto screens = list.screens(cinemas[0].first);
		BOOST_REQUIRE_EQUAL(screens.size(), 1U);
		BOOST_CHECK_EQUAL(screens[0].second.name, "Screen 1");
		BOOST_CHECK_EQUAL(screens[0].second.notes, "GDC SR1000");
		BOOST_CHECK(screens[0].second.recipient());
	}

	/* list-cinemas shows the screens only with --verbose */
	output.clear();
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "list-cinemas"}, output));
	BOOST_CHECK_EQUAL(output.size(), 1U);
	output.clear();
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "--verbose", "list-cinemas"}, output));
	BOOST_REQUIRE_EQUAL(output.size(), 2U);
	BOOST_CHECK_EQUAL(output[1], "    Screen 1");

	/* Refused: same screen again, unknown cinema, no certificate, two certificates, a bad --certificate-from */
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "-C", cert, "add-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Nowhere", "-S", "Screen 2", "-C", cert, "add-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "add-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "-C", cert, "--certificate-from", "GDC:A1", "add-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "--certificate-from", "GDC", "add-screen"}, output));
	CinemaList list(file);
	BOOST_CHECK_EQUAL(list.screens(list.cinemas()[0].first).size(), 1U);
}


BOOST_AUTO_TEST_CASE(kdm_cli_list_screens_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_list_screens_test");
	vector<string> output;
	auto const cert = string("test/data/cert.pem");

	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "-C", cert, "--notes", "GDC SR1000", "add-screen"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "-C", cert, "add-screen"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "-S", "Elsewhere", "-C", cert, "add-screen"}, output));

	output.clear();
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "list-screens"}, output));
	/* Only the screens of that cinema, with the common name of the certificate and the notes */
	BOOST_REQUIRE_EQUAL(output.size(), 2U);
	BOOST_CHECK(output[0].find("Screen 1: ") == 0);
	BOOST_CHECK(output[0].find(" [GDC SR1000]") != string::npos);
	BOOST_CHECK(output[1].find("Screen 2: ") == 0);
	BOOST_CHECK(output[1].find("[") == string::npos);

	output.clear();
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "--verbose", "-c", "Fred's Cinema", "list-screens"}, output));
	BOOST_REQUIRE_EQUAL(output.size(), 2U);
	BOOST_CHECK(output[0].find(" (valid ") != string::npos);

	/* A cinema is needed and it must exist */
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "list-screens"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Nowhere", "list-screens"}, output));
}


BOOST_AUTO_TEST_CASE(kdm_cli_edit_cinema_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_edit_cinema_test");
	vector<string> output;

	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "--emails", "fred@a.com", "--notes", "back door", "--utc-offset", "+1", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "add-cinema"}, output));

	/* Only what is given changes */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "--utc-offset", "-5:30", "edit-cinema"}, output));
	{
		CinemaList list(file);
		auto cinema = list.cinema_by_name_or_email("Fred's Cinema");
		BOOST_REQUIRE(cinema);
		BOOST_CHECK_EQUAL(cinema->second.notes, "back door");
		BOOST_REQUIRE_EQUAL(cinema->second.emails.size(), 1U);
		BOOST_CHECK_EQUAL(cinema->second.utc_offset.hour(), -5);
		BOOST_CHECK_EQUAL(cinema->second.utc_offset.minute(), -30);
	}

	/* An empty value clears; rename */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "--emails", "", "--notes", "", "--new-name", "Joe's Cinema", "edit-cinema"}, output));
	{
		CinemaList list(file);
		BOOST_CHECK(!list.cinema_by_name_or_email("Fred's Cinema"));
		auto cinema = list.cinema_by_name_or_email("Joe's Cinema");
		BOOST_REQUIRE(cinema);
		/* CinemaList reads an empty list of addresses back as one empty address */
		BOOST_CHECK(std::all_of(cinema->second.emails.begin(), cinema->second.emails.end(), [](string const& e) { return e.empty(); }));
		BOOST_CHECK(cinema->second.notes.empty());
		BOOST_CHECK_EQUAL(list.cinemas().size(), 2U);
	}

	/* Refused: nothing to change, unknown cinema, a name already taken, an empty name, a bad offset */
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Joe's Cinema", "edit-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Nowhere", "--notes", "x", "edit-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Joe's Cinema", "--new-name", "Other", "edit-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Joe's Cinema", "--new-name", "", "edit-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Joe's Cinema", "--utc-offset", "abc", "edit-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "--notes", "x", "edit-cinema"}, output));
}


BOOST_AUTO_TEST_CASE(kdm_cli_edit_screen_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_edit_screen_test");
	vector<string> output;
	auto const cert = string("test/data/cert.pem");
	auto const cert2 = string("test/data/cert2.pem");

	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "-C", cert, "--notes", "GDC", "add-screen"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "-C", cert, "add-screen"}, output));

	string thumbprint;
	{
		CinemaList list(file);
		auto screens = list.screens_by_cinema_and_name(list.cinemas()[0].first, "Screen 1");
		BOOST_REQUIRE_EQUAL(screens.size(), 1U);
		thumbprint = screens[0].second.recipient()->thumbprint();
	}

	/* Notes only: the certificate stays */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "--notes", "Barco", "edit-screen"}, output));
	{
		CinemaList list(file);
		auto screens = list.screens_by_cinema_and_name(list.cinemas()[0].first, "Screen 1");
		BOOST_REQUIRE_EQUAL(screens.size(), 1U);
		BOOST_CHECK_EQUAL(screens[0].second.notes, "Barco");
		BOOST_CHECK_EQUAL(screens[0].second.recipient()->thumbprint(), thumbprint);
	}

	/* Rename and new certificate */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "--new-name", "Main", "-C", cert2, "edit-screen"}, output));
	{
		CinemaList list(file);
		auto const id = list.cinemas()[0].first;
		BOOST_CHECK(list.screens_by_cinema_and_name(id, "Screen 1").empty());
		auto screens = list.screens_by_cinema_and_name(id, "Main");
		BOOST_REQUIRE_EQUAL(screens.size(), 1U);
		BOOST_CHECK_EQUAL(screens[0].second.notes, "Barco");
		BOOST_CHECK(screens[0].second.recipient()->thumbprint() != thumbprint);
		BOOST_CHECK_EQUAL(list.screens(id).size(), 2U);
	}

	/* Refused: nothing to change, unknown cinema or screen, a name already taken, two certificates, a bad --certificate-from */
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Main", "edit-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Nowhere", "-S", "Main", "--notes", "x", "edit-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Nothing", "--notes", "x", "edit-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Main", "--new-name", "Screen 2", "edit-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Main", "-C", cert, "--certificate-from", "GDC:A1", "edit-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Main", "--certificate-from", "GDC", "edit-screen"}, output));
}


BOOST_AUTO_TEST_CASE(kdm_cli_remove_cinema_screen_test)
{
	auto const file = fresh_cinemas_file("kdm_cli_remove_cinema_screen_test");
	vector<string> output;
	auto const cert = string("test/data/cert.pem");

	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "add-cinema"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "-C", cert, "add-screen"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 2", "-C", cert, "add-screen"}, output));
	BOOST_REQUIRE(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "-S", "Elsewhere", "-C", cert, "add-screen"}, output));

	/* One screen goes, the rest stay */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "remove-screen"}, output));
	{
		CinemaList list(file);
		auto const id = list.cinema_by_name_or_email("Fred's Cinema")->first;
		BOOST_REQUIRE_EQUAL(list.screens(id).size(), 1U);
		BOOST_CHECK_EQUAL(list.screens(id)[0].second.name, "Screen 2");
	}
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "-S", "Screen 1", "remove-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Nowhere", "-S", "Screen 2", "remove-screen"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "remove-screen"}, output));

	/* A cinema with screens is not removed without --with-screens */
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Fred's Cinema", "remove-cinema"}, output));
	{
		CinemaList list(file);
		BOOST_CHECK_EQUAL(list.cinemas().size(), 2U);
	}

	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "--with-screens", "-c", "Fred's Cinema", "remove-cinema"}, output));
	{
		CinemaList list(file);
		BOOST_REQUIRE_EQUAL(list.cinemas().size(), 1U);
		BOOST_CHECK_EQUAL(list.cinemas()[0].second.name, "Other");
		/* Its screens went with it and the other cinema's screen is still there */
		int count = 0;
		list.screens([&count](CinemaID, ScreenID, dcpomatic::Screen const&) { ++count; }, false);
		BOOST_CHECK_EQUAL(count, 1);
	}

	/* A cinema without screens needs no flag; an unknown one is refused */
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "-S", "Elsewhere", "remove-screen"}, output));
	BOOST_CHECK(!run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "remove-cinema"}, output));
	BOOST_CHECK(run_cli({"kdm_cli", "--cinemas-file", file.string(), "-c", "Other", "remove-cinema"}, output));
	CinemaList list(file);
	BOOST_CHECK(list.cinemas().empty());
}
