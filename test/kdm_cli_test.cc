/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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
#include "lib/kdm_cli.h"
#include "lib/screen.h"
#include "lib/trusted_device.h"
#include "test.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::list;
using std::string;
using std::vector;
using boost::optional;


optional<string>
run(vector<string> const& args, vector<string>& output)
{
	std::vector<char*> argv(args.size());
	for (auto i = 0U; i < args.size(); ++i) {
		argv[i] = const_cast<char*>(args[i].c_str());
	}

	auto error = kdm_cli(args.size(), argv.data(), [&output](string s) { output.push_back(s); });
	if (error) {
		std::cout << *error << "\n";
	}

	return error;
}


BOOST_AUTO_TEST_CASE (kdm_cli_test_certificate)
{
	vector<string> args = {
		"kdm_cli",
		"--verbose",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"--certificate", "test/data/cert.pem",
		"-S", "my great screen",
		"-o", "build/test",
		"test/data/dkdm.xml"
	};

	boost::filesystem::path const kdm_filename = "build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV__my_great_screen.xml";
	boost::system::error_code ec;
	boost::filesystem::remove(kdm_filename, ec);

	vector<string> output;
	auto error = run(args, output);
	BOOST_CHECK (!error);

	BOOST_CHECK(boost::filesystem::exists(kdm_filename));
}


static
void
setup_test_config()
{
	auto config = Config::instance();
	auto const cert = dcp::Certificate(dcp::file_to_string("test/data/cert.pem"));

	auto cinema_a = std::make_shared<Cinema>("Dean's Screens", list<string>(), "", 0, 0);
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 1", "", cert, boost::none, std::vector<TrustedDevice>()));
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 2", "", cert, boost::none, std::vector<TrustedDevice>()));
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 3", "", cert, boost::none, std::vector<TrustedDevice>()));
	config->add_cinema(cinema_a);

	auto cinema_b = std::make_shared<Cinema>("Floyd's Celluloid", list<string>(), "", 0, 0);
	cinema_b->add_screen(std::make_shared<dcpomatic::Screen>("Foo", "", cert, boost::none, std::vector<TrustedDevice>()));
	cinema_b->add_screen(std::make_shared<dcpomatic::Screen>("Bar", "", cert, boost::none, std::vector<TrustedDevice>()));
	config->add_cinema(cinema_b);
}


BOOST_AUTO_TEST_CASE(kdm_cli_select_cinema)
{
	ConfigRestorer cr;

	setup_test_config();

	vector<boost::filesystem::path> kdm_filenames = {
		"build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV_Floyds_Celluloid_Foo.xml",
		"build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV_Floyds_Celluloid_Bar.xml"
	};

	for (auto path: kdm_filenames) {
		boost::system::error_code ec;
		boost::filesystem::remove(path, ec);
	}

	vector<string> args = {
		"kdm_cli",
		"--verbose",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"-c", "Floyd's Celluloid",
		"-o", "build/test",
		"test/data/dkdm.xml"
	};

	vector<string> output;
	auto error = run(args, output);
	BOOST_CHECK(!error);

	BOOST_REQUIRE_EQUAL(output.size(), 2U);
	BOOST_CHECK(boost::algorithm::starts_with(output[0], "Making KDMs valid from"));
	BOOST_CHECK_EQUAL(output[1], "Wrote 2 KDM files to build/test");

	for (auto path: kdm_filenames) {
		BOOST_CHECK(boost::filesystem::exists(path));
	}
}


BOOST_AUTO_TEST_CASE(kdm_cli_select_screen)
{
	ConfigRestorer cr;

	setup_test_config();

	boost::filesystem::path kdm_filename = "build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV_Deans_Screens_Screen_2.xml";

	boost::system::error_code ec;
	boost::filesystem::remove(kdm_filename, ec);

	vector<string> args = {
		"kdm_cli",
		"--verbose",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"-c", "Dean's Screens",
		"-S", "Screen 2",
		"-o", "build/test",
		"test/data/dkdm.xml"
	};

	vector<string> output;
	auto error = run(args, output);
	BOOST_CHECK(!error);

	BOOST_REQUIRE_EQUAL(output.size(), 2U);
	BOOST_CHECK(boost::algorithm::starts_with(output[0], "Making KDMs valid from"));
	BOOST_CHECK_EQUAL(output[1], "Wrote 1 KDM files to build/test");

	BOOST_CHECK(boost::filesystem::exists(kdm_filename));
}


