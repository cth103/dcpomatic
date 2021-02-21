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

#include "lib/cinema.h"
#include "lib/screen.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/kdm_with_metadata.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::list;
using std::string;
using std::vector;
using std::shared_ptr;
using boost::optional;
using std::dynamic_pointer_cast;

static
bool
confirm_overwrite (boost::filesystem::path)
{
	return true;
}

static shared_ptr<dcpomatic::Screen> cinema_a_screen_1;
static shared_ptr<dcpomatic::Screen> cinema_a_screen_2;
static shared_ptr<dcpomatic::Screen> cinema_b_screen_x;
static shared_ptr<dcpomatic::Screen> cinema_b_screen_y;
static shared_ptr<dcpomatic::Screen> cinema_b_screen_z;

BOOST_AUTO_TEST_CASE (single_kdm_naming_test)
{
	Config* c = Config::instance();

	dcp::Certificate cert = c->decryption_chain()->leaf();

	/* Cinema A: UTC +4:30 */
	shared_ptr<Cinema> cinema_a (new Cinema("Cinema A", list<string>(), "", 4, 30));
	cinema_a_screen_1.reset(new dcpomatic::Screen("Screen 1", "", cert, vector<TrustedDevice>()));
	cinema_a->add_screen (cinema_a_screen_1);
	cinema_a_screen_2.reset(new dcpomatic::Screen("Screen 2", "", cert, vector<TrustedDevice>()));
	cinema_a->add_screen (cinema_a_screen_2);
	c->add_cinema (cinema_a);

	/* Cinema B: UTC -1:00 */
	shared_ptr<Cinema> cinema_b (new Cinema("Cinema B", list<string>(), "", -1, 0));
	cinema_b_screen_x.reset(new dcpomatic::Screen("Screen X", "", cert, vector<TrustedDevice>()));
	cinema_b->add_screen (cinema_b_screen_x);
	cinema_b_screen_y.reset(new dcpomatic::Screen("Screen Y", "", cert, vector<TrustedDevice>()));
	cinema_b->add_screen (cinema_b_screen_y);
	cinema_b_screen_z.reset(new dcpomatic::Screen("Screen Z", "", cert, vector<TrustedDevice>()));
	cinema_b->add_screen (cinema_b_screen_z);
	c->add_cinema (cinema_a);

	/* Film */
	boost::filesystem::remove_all ("build/test/single_kdm_naming_test");
	shared_ptr<Film> film = new_test_film2 ("single_kdm_naming_test");
	film->set_name ("my_great_film");
	film->examine_and_add_content (content_factory("test/data/flat_black.png").front());
	BOOST_REQUIRE (!wait_for_jobs());
	film->set_encrypted (true);
	make_and_verify_dcp (film);
	auto cpls = film->cpls ();
	BOOST_REQUIRE(cpls.size() == 1);

	dcp::LocalTime from (cert.not_before());
	from.add_months (2);
	dcp::LocalTime until (cert.not_after());
	until.add_months (-2);

	string const from_string = from.date() + " " + from.time_of_day(true, false);
	string const until_string = until.date() + " " + until.time_of_day(true, false);

	auto cpl = cpls.front().cpl_file;
	auto kdm = kdm_for_screen (
			film,
			cpls.front().cpl_file,
			cinema_a_screen_1,
			boost::posix_time::time_from_string(from_string),
			boost::posix_time::time_from_string(until_string),
			dcp::Formulation::MODIFIED_TRANSITIONAL_1,
			false,
			optional<int>()
			);

	write_files (
		{ kdm },
		boost::filesystem::path("build/test/single_kdm_naming_test"),
		dcp::NameFormat("KDM %c - %s - %f - %b - %e"),
		&confirm_overwrite
		);

	string from_time = from.time_of_day (true, false);
	boost::algorithm::replace_all (from_time, ":", "-");
	string until_time = until.time_of_day (true, false);
	boost::algorithm::replace_all (until_time, ":", "-");

	string const ref = String::compose("KDM_Cinema_A_-_Screen_1_-_my_great_film_-_%1_%2_-_%3_%4.xml", from.date(), from_time, until.date(), until_time);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists("build/test/single_kdm_naming_test/" + ref), "File " << ref << " not found");
}


BOOST_AUTO_TEST_CASE (directory_kdm_naming_test, * boost::unit_test::depends_on("single_kdm_naming_test"))
{
	using boost::filesystem::path;

	dcp::Certificate cert = Config::instance()->decryption_chain()->leaf();

	/* Film */
	boost::filesystem::remove_all ("build/test/directory_kdm_naming_test");
	auto film = new_test_film2 (
		"directory_kdm_naming_test",
		{ content_factory("test/data/flat_black.png").front() }
		);

	film->set_name ("my_great_film");
	film->set_encrypted (true);
	make_and_verify_dcp (film);
	auto cpls = film->cpls ();
	BOOST_REQUIRE(cpls.size() == 1);

	dcp::LocalTime from (cert.not_before());
	from.add_months (2);
	dcp::LocalTime until (cert.not_after());
	until.add_months (-2);

	string const from_string = from.date() + " " + from.time_of_day(true, false);
	string const until_string = until.date() + " " + until.time_of_day(true, false);

	list<shared_ptr<dcpomatic::Screen>> screens = {
		cinema_a_screen_2, cinema_b_screen_x, cinema_a_screen_1, (cinema_b_screen_z)
	};

	auto const cpl = cpls.front().cpl_file;
	auto const cpl_id = cpls.front().cpl_id;

	list<KDMWithMetadataPtr> kdms;
	for (auto i: screens) {
		auto kdm = kdm_for_screen (
				film,
				cpls.front().cpl_file,
				i,
				boost::posix_time::time_from_string(from_string),
				boost::posix_time::time_from_string(until_string),
				dcp::Formulation::MODIFIED_TRANSITIONAL_1,
				false,
				optional<int>()
				);

		kdms.push_back (kdm);
	}

	write_directories (
		collect(kdms),
		path("build/test/directory_kdm_naming_test"),
		dcp::NameFormat("%c - %s - %f - %b - %e"),
#ifdef DCPOMATIC_WINDOWS
		/* With %i in the format the path is too long for Windows */
		dcp::NameFormat("KDM %c - %s - %f - %b - %e"),
#else
		dcp::NameFormat("KDM %c - %s - %f - %b - %e - %i"),
#endif
		&confirm_overwrite
		);

	auto from_time = from.time_of_day (true, false);
	boost::algorithm::replace_all (from_time, ":", "-");
	auto until_time = until.time_of_day (true, false);
	boost::algorithm::replace_all (until_time, ":", "-");

	path const base = "build/test/directory_kdm_naming_test";

	path dir_a = String::compose("Cinema_A_-_%s_-_my_great_film_-_%1_%2_-_%3_%4", from.date(), from_time, until.date(), until_time);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a), "Directory " << dir_a << " not found");
	path dir_b = String::compose("Cinema_B_-_%s_-_my_great_film_-_%1_%2_-_%3_%4", from.date(), from_time, until.date(), until_time);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b), "Directory " << dir_b << " not found");

#ifdef DCPOMATIC_WINDOWS
	path ref = String::compose("KDM_Cinema_A_-_Screen_2_-_my_great_film_-_%1_%2_-_%3_%4.xml", from.date(), from_time, until.date(), until_time);
#else
	path ref = String::compose("KDM_Cinema_A_-_Screen_2_-_my_great_film_-_%1_%2_-_%3_%4_-_%5.xml", from.date(), from_time, until.date(), until_time, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_Cinema_B_-_Screen_X_-_my_great_film_-_%1_%2_-_%3_%4.xml", from.date(), from_time, until.date(), until_time);
#else
	ref = String::compose("KDM_Cinema_B_-_Screen_X_-_my_great_film_-_%1_%2_-_%3_%4_-_%5.xml", from.date(), from_time, until.date(), until_time, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_Cinema_A_-_Screen_1_-_my_great_film_-_%1_%2_-_%3_%4.xml", from.date(), from_time, until.date(), until_time);
#else
	ref = String::compose("KDM_Cinema_A_-_Screen_1_-_my_great_film_-_%1_%2_-_%3_%4_-_%5.xml", from.date(), from_time, until.date(), until_time, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_Cinema_B_-_Screen_Z_-_my_great_film_-_%1_%2_-_%3_%4.xml", from.date(), from_time, until.date(), until_time);
#else
	ref = String::compose("KDM_Cinema_B_-_Screen_Z_-_my_great_film_-_%1_%2_-_%3_%4_-_%5.xml", from.date(), from_time, until.date(), until_time, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");
}

