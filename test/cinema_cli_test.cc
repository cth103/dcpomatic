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
