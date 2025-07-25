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
#include "lib/cinema_list.h"
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
using std::pair;
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


struct Context
{
	Context()
	{
		CinemaList cinemas;

		auto crypt_cert = Config::instance()->decryption_chain()->leaf();

		cinema_a = cinemas.add_cinema({"Cinema A", {}, "", dcp::UTCOffset(4, 30)});
		cinema_a_screen_1 = cinemas.add_screen(cinema_a, {"Screen 1", "", crypt_cert, boost::none, {}});
		cinema_a_screen_2 = cinemas.add_screen(cinema_a, {"Screen 2", "", crypt_cert, boost::none, {}});

		cinema_b = cinemas.add_cinema({"Cinema B", {}, "", dcp::UTCOffset(-1, 0)});
		cinema_b_screen_x = cinemas.add_screen(cinema_b, {"Screen X", "", crypt_cert, boost::none, {}});
		cinema_b_screen_y = cinemas.add_screen(cinema_b, {"Screen Y", "", crypt_cert, boost::none, {}});
		cinema_b_screen_z = cinemas.add_screen(cinema_b, {"Screen Z", "", crypt_cert, boost::none, {}});
	}

	CinemaID cinema_a = 0;
	CinemaID cinema_b = 0;
	ScreenID cinema_a_screen_1 = 0;
	ScreenID cinema_a_screen_2 = 0;
	ScreenID cinema_b_screen_x = 0;
	ScreenID cinema_b_screen_y = 0;
	ScreenID cinema_b_screen_z = 0;
};


BOOST_AUTO_TEST_CASE (single_kdm_naming_test)
{
	auto c = Config::instance();

	Context context;
	CinemaList cinemas;

	/* Film */
	boost::filesystem::remove_all ("build/test/single_kdm_naming_test");
	auto film = new_test_film("single_kdm_naming_test");
	film->set_name ("my_great_film");
	film->examine_and_add_content(content_factory("test/data/flat_black.png"));
	BOOST_REQUIRE (!wait_for_jobs());
	film->set_encrypted (true);
	make_and_verify_dcp (film);
	auto cpls = film->cpls ();
	BOOST_REQUIRE(cpls.size() == 1);

	auto sign_cert = c->signer_chain()->leaf();

	dcp::LocalTime from = sign_cert.not_before();
	from.add_months (2);
	dcp::LocalTime until = sign_cert.not_after();
	until.add_months (-2);

	auto const from_string = from.date() + " " + from.time_of_day(true, false);
	auto const until_string = until.date() + " " + until.time_of_day(true, false);

	std::vector<KDMCertificatePeriod> period_checks;

	auto cpl = cpls.front().cpl_file;
	std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [film, cpl](dcp::LocalTime begin, dcp::LocalTime end) {
		return film->make_kdm(cpl, begin, end);
	};
	auto kdm = kdm_for_screen (
			make_kdm,
			context.cinema_a,
			*cinemas.cinema(context.cinema_a),
			*cinemas.screen(context.cinema_a_screen_1),
			boost::posix_time::time_from_string(from_string),
			boost::posix_time::time_from_string(until_string),
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
	auto const ref = fmt::format("KDM_Cinema_A_-_Screen_1_-_MyGreatFilm_TST-1_F_XX-XX_MOS_2K_{}_SMPTE_OV_-_{}_{}_-_{}_{}.xml", dcp_date, from.date(), from_time, until.date(), until_time);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists("build/test/single_kdm_naming_test/" + ref), "File " << ref << " not found");
}


BOOST_AUTO_TEST_CASE(directory_kdm_naming_test)
{
	using boost::filesystem::path;

	Context context;
	CinemaList cinemas;

	/* Film */
	boost::filesystem::remove_all ("build/test/directory_kdm_naming_test");
	auto film = new_test_film(
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

	auto const from_string = from.date() + " " + from.time_of_day(true, false);
	auto const until_string = until.date() + " " + until.time_of_day(true, false);

	vector<pair<CinemaID, ScreenID>> screens = {
		{ context.cinema_a, context.cinema_a_screen_2 },
		{ context.cinema_b, context.cinema_b_screen_x },
		{ context.cinema_a, context.cinema_a_screen_1 },
		{ context.cinema_b, context.cinema_b_screen_z }
	};

	auto const cpl = cpls.front().cpl_file;
	auto const cpl_id = cpls.front().cpl_id;

	std::vector<KDMCertificatePeriod> period_checks;
	list<KDMWithMetadataPtr> kdms;

	std::function<dcp::DecryptedKDM (dcp::LocalTime, dcp::LocalTime)> make_kdm = [film, cpls](dcp::LocalTime begin, dcp::LocalTime end) {
		return film->make_kdm(cpls.front().cpl_file, begin, end);
	};

	for (auto screen: screens) {
		auto kdm = kdm_for_screen (
				make_kdm,
				screen.first,
				*cinemas.cinema(screen.first),
				*cinemas.screen(screen.second),
				boost::posix_time::time_from_string(from_string),
				boost::posix_time::time_from_string(until_string),
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
	auto const dcp_name = fmt::format("MyGreatFilm_TST-1_F_XX-XX_MOS_2K_{}_SMPTE_OV", dcp_date);
	auto const common = fmt::format("{}_-_{}_{}_-_{}_{}", dcp_name, from.date(), from_time, until.date(), until_time);

	path const base = "build/test/directory_kdm_naming_test";

	path dir_a = fmt::format("Cinema_A_-_%s_-_{}", common);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a), "Directory " << dir_a << " not found");
	path dir_b = fmt::format("Cinema_B_-_%s_-_{}", common);
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b), "Directory " << dir_b << " not found");

#ifdef DCPOMATIC_WINDOWS
	path ref = fmt::format("KDM_{}.xml", dcp_name);
#else
	path ref = fmt::format("KDM_Cinema_A_-_Screen_2_-_{}_-_{}.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = fmt::format("KDM_{}.xml", dcp_name);
#else
	ref = fmt::format("KDM_Cinema_B_-_Screen_X_-_{}_-_{}.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = fmt::format("KDM_{}.xml", dcp_name);
#else
	ref = fmt::format("KDM_Cinema_A_-_Screen_1_-_{}_-_{}.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_a / ref), "File " << ref << " not found");

#ifdef DCPOMATIC_WINDOWS
	ref = fmt::format("KDM_{}.xml", dcp_name);
#else
	ref = fmt::format("KDM_Cinema_B_-_Screen_Z_-_{}_-_{}.xml", common, cpl_id);
#endif
	BOOST_CHECK_MESSAGE (boost::filesystem::exists(base / dir_b / ref), "File " << ref << " not found");
}

