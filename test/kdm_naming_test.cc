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


#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/kdm_with_metadata.h"
#include "lib/screen.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


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
	auto c = Config::instance();

	auto crypt_cert = c->decryption_chain()->leaf();

	auto cinema_a = make_shared<Cinema>("Cinema A", vector<string>(), "");
	cinema_a_screen_1 = std::make_shared<dcpomatic::Screen>("Screen 1", "", crypt_cert, boost::none, vector<TrustedDevice>());
	cinema_a->add_screen (cinema_a_screen_1);
	cinema_a_screen_2 = std::make_shared<dcpomatic::Screen>("Screen 2", "", crypt_cert, boost::none, vector<TrustedDevice>());
	cinema_a->add_screen (cinema_a_screen_2);
	c->add_cinema (cinema_a);

	auto cinema_b = make_shared<Cinema>("Cinema B", vector<string>(), "");
	cinema_b_screen_x = std::make_shared<dcpomatic::Screen>("Screen X", "", crypt_cert, boost::none, vector<TrustedDevice>());
	cinema_b->add_screen (cinema_b_screen_x);
	cinema_b_screen_y = std::make_shared<dcpomatic::Screen>("Screen Y", "", crypt_cert, boost::none, vector<TrustedDevice>());
	cinema_b->add_screen (cinema_b_screen_y);
	cinema_b_screen_z = std::make_shared<dcpomatic::Screen>("Screen Z", "", crypt_cert, boost::none, vector<TrustedDevice>());
	cinema_b->add_screen (cinema_b_screen_z);
	c->add_cinema (cinema_a);

	/* Film */
	boost::filesystem::remove_all ("build/test/single_kdm_naming_test");
	auto film = new_test_film2 ("single_kdm_naming_test");
	film->set_name ("my_great_film");
	film->examine_and_add_content (content_factory("test/data/flat_black.png")[0]);
	BOOST_REQUIRE (!wait_for_jobs());
	film->set_encrypted (true);
	make_and_verify_dcp (film);
	auto cpls = film->cpls ();
	BOOST_REQUIRE(cpls.size() == 1);

	auto sign_cert = c->signer_chain()->leaf();

	dcp::LocalTime from = sign_cert.not_before();
	from.set_offset({ 4, 30 });
	from.add_months (2);
	dcp::LocalTime until = sign_cert.not_after();
	until.set_offset({ 4, 30 });
	until.add_months (-2);

	std::vector<KDMCertificatePeriod> period_checks;

	auto cpl = cpls.front().cpl_file;
	std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [film, cpl](dcp::LocalTime begin, dcp::LocalTime end) {
		return film->make_kdm(cpl, begin, end);
	};
	auto kdm = kdm_for_screen (
			make_kdm,
			cinema_a_screen_1,
			from,
			until,
			dcp::Formulation::MODIFIED_TRANSITIONAL_1,
			false,
			optional<int>(),
			period_checks
			);

	write_files (
		{ kdm },
		boost::filesystem::path("build/test/single_kdm_naming_test"),
		dcp::NameFormat("KDM %c - %s - %f - %b - %e"),
		&confirm_overwrite
		);

	auto from_time = from.time_of_day (true, false);
	boost::algorithm::replace_all (from_time, ":", "-");
	auto until_time = until.time_of_day (true, false);
	boost::algorithm::replace_all (until_time, ":", "-");

	auto const dcp_date = boost::gregorian::to_iso_string(film->isdcf_date());
	auto const ref = String::compose("KDM_Cinema_A_-_Screen_1_-_MyGreatFilm_TST-1_F_XX-XX_MOS_2K_%1_SMPTE_OV_-_%2_%3_-_%4_%5.xml", dcp_date, from.date(), from_time, until.date(), until_time);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists("build/test/single_kdm_naming_test/" + ref), "File " << ref << " not found");
}


BOOST_AUTO_TEST_CASE (directory_kdm_naming_test, * boost::unit_test::depends_on("single_kdm_naming_test"))
{
	using boost::filesystem::path;

	/* Film */
	boost::filesystem::remove_all ("build/test/directory_kdm_naming_test");
	auto film = new_test_film2 (
		"directory_kdm_naming_test",
		{ content_factory("test/data/flat_black.png")[0] }
		);

	film->set_name ("my_great_film");
	film->set_encrypted (true);
	make_and_verify_dcp (film);
	auto cpls = film->cpls ();
	BOOST_REQUIRE(cpls.size() == 1);

	auto sign_cert = Config::instance()->signer_chain()->leaf();

	dcp::LocalTime from (sign_cert.not_before());
	from.add_months (2);
	dcp::LocalTime until (sign_cert.not_after());
	until.add_months (-2);

	vector<shared_ptr<dcpomatic::Screen>> screens = {
		cinema_a_screen_2, cinema_b_screen_x, cinema_a_screen_1, (cinema_b_screen_z)
	};

	auto const cpl = cpls.front().cpl_file;
	auto const cpl_id = cpls.front().cpl_id;

	std::vector<KDMCertificatePeriod> period_checks;
	list<KDMWithMetadataPtr> kdms;

	std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [film, cpls](dcp::LocalTime begin, dcp::LocalTime end) {
		return film->make_kdm(cpls.front().cpl_file, begin, end);
	};

	for (auto i: screens) {
		auto kdm = kdm_for_screen (
				make_kdm,
				i,
				from,
				until,
				dcp::Formulation::MODIFIED_TRANSITIONAL_1,
				false,
				optional<int>(),
				period_checks
				);

		kdms.push_back (kdm);
	}

	write_directories (
		collect(kdms),
		path("build/test/directory_kdm_naming_test"),
		dcp::NameFormat("%c - %s - %f - %b - %e"),
#ifdef DCPOMATIC_WINDOWS
		/* Use a shorter name on Windows so that the paths aren't too long */
		dcp::NameFormat("KDM %f"),
#else
		dcp::NameFormat("KDM %c - %s - %f - %b - %e - %i"),
#endif
		&confirm_overwrite
		);

	auto from_time = from.time_of_day (true, false);
	boost::algorithm::replace_all (from_time, ":", "-");
	auto until_time = until.time_of_day (true, false);
	boost::algorithm::replace_all (until_time, ":", "-");

	auto const dcp_date = boost::gregorian::to_iso_string(film->isdcf_date());
	auto const dcp_name = String::compose("MyGreatFilm_TST-1_F_XX-XX_MOS_2K_%1_SMPTE_OV", dcp_date);
	auto const common = String::compose("%1_-_%2_%3_-_%4_%5", dcp_name, from.date(), from_time, until.date(), until_time);

	path const base = "build/test/directory_kdm_naming_test";

	path dir_a = String::compose("Cinema_A_-_%s_-_%1", common);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a), "Directory " << dir_a << " not found");
	path dir_b = String::compose("Cinema_B_-_%s_-_%1", common);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b), "Directory " << dir_b << " not found");

#ifdef DCPOMATIC_WINDOWS
	path ref = String::compose("KDM_%1.xml", dcp_name);
#else
	path ref = String::compose("KDM_Cinema_A_-_Screen_2_-_%1_-_%2.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_%1.xml", dcp_name);
#else
	ref = String::compose("KDM_Cinema_B_-_Screen_X_-_%1_-_%2.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_%1.xml", dcp_name);
#else
	ref = String::compose("KDM_Cinema_A_-_Screen_1_-_%1_-_%2.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = String::compose("KDM_%1.xml", dcp_name);
#else
	ref = String::compose("KDM_Cinema_B_-_Screen_Z_-_%1_-_%2.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");
}

