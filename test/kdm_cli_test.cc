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
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dkdm_wrapper.h"
#include "lib/film.h"
#include "lib/kdm_cli.h"
#include "lib/screen.h"
#include "lib/trusted_device.h"
#include "test.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::dynamic_pointer_cast;
using std::string;
using std::vector;
using boost::optional;


optional<string>
run(vector<string> const& args, vector<string>& output, bool dump_errors = true)
{
	std::vector<char*> argv(args.size());
	for (auto i = 0U; i < args.size(); ++i) {
		argv[i] = const_cast<char*>(args[i].c_str());
	}

	auto error = kdm_cli(args.size(), argv.data(), [&output](string s) { output.push_back(s); });
	if (error && dump_errors) {
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
		"--projector-certificate", "test/data/cert.pem",
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


BOOST_AUTO_TEST_CASE(kdm_cli_specify_decryption_key_test)
{
	using boost::filesystem::path;

	ConfigRestorer cr;

	path const dir = "build/test/kdm_cli_specify_decryption_key_test";

	boost::system::error_code ec;
	boost::filesystem::remove_all(dir, ec);
	boost::filesystem::create_directories(dir);

	dcp::CertificateChain chain(openssl_path(), 365);
	dcp::write_string_to_file(chain.leaf().certificate(true), dir / "cert.pem");
	dcp::write_string_to_file(*chain.key(), dir / "key.pem");

	vector<string> make_args = {
		"kdm_cli",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"--projector-certificate", path(dir / "cert.pem").string(),
		"-S", "base",
		"-o", dir.string(),
		"test/data/dkdm.xml"
	};

	vector<string> output;
	auto error = run(make_args, output);
	BOOST_CHECK(!error);

	vector<string> bad_args = {
		"kdm_cli",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"--projector-certificate", path(dir / "cert.pem").string(),
		"-S", "bad",
		"-o", dir.string(),
		path(dir / "KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV__base.xml").string()
	};

	/* This should fail because we're using the wrong decryption certificate */
	output.clear();
	error = run(bad_args, output, false);
	BOOST_REQUIRE(error);
	BOOST_CHECK_MESSAGE(error->find("Could not decrypt KDM") != string::npos, "Error was " << *error);

	vector<string> good_args = {
		"kdm_cli",
		"--valid-from", "now",
		"--valid-duration", "2 weeks",
		"--projector-certificate", path(dir / "cert.pem").string(),
		"--decryption-key", path(dir / "key.pem").string(),
		"-S", "good",
		"-o", dir.string(),
		path(dir / "KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV__base.xml").string()
	};

	/* This should succeed */
	output.clear();
	error = run(good_args, output);
	BOOST_CHECK(!error);
}


static
void
setup_test_config()
{
	auto config = Config::instance();
	auto const cert = dcp::Certificate(dcp::file_to_string("test/data/cert.pem"));

	auto cinema_a = std::make_shared<Cinema>("Dean's Screens", vector<string>(), "");
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 1", "", cert, boost::none, std::vector<TrustedDevice>()));
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 2", "", cert, boost::none, std::vector<TrustedDevice>()));
	cinema_a->add_screen(std::make_shared<dcpomatic::Screen>("Screen 3", "", cert, boost::none, std::vector<TrustedDevice>()));
	config->add_cinema(cinema_a);

	auto cinema_b = std::make_shared<Cinema>("Floyd's Celluloid", vector<string>(), "");
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


BOOST_AUTO_TEST_CASE(kdm_cli_specify_cinemas_file)
{
	ConfigRestorer cr;

	setup_test_config();

	vector<string> args = {
		"kdm_cli",
		"--cinemas-file",
		"test/data/cinemas.xml",
		"list-cinemas"
	};

	vector<string> output;
	auto const error = run(args, output);
	BOOST_CHECK(!error);

	BOOST_REQUIRE_EQUAL(output.size(), 3U);
	BOOST_CHECK_EQUAL(output[0], "stinking dump ()");
	BOOST_CHECK_EQUAL(output[1], "classy joint ()");
	BOOST_CHECK_EQUAL(output[2], "Great ()");
}


BOOST_AUTO_TEST_CASE(kdm_cli_specify_cert)
{
	boost::filesystem::path kdm_filename = "build/test/KDM_KDMCLI__.xml";

	boost::system::error_code ec;
	boost::filesystem::remove(kdm_filename, ec);

	auto film = new_test_film2("kdm_cli_specify_cert", content_factory("test/data/flat_red.png"));
	film->set_encrypted(true);
	film->set_name("KDMCLI");
	film->set_use_isdcf_name(false);
	make_and_verify_dcp(film);

	vector<string> args = {
		"kdm_cli",
		"--valid-from", "2024-01-01 10:10:10",
		"--valid-duration", "2 weeks",
		"-C", "test/data/cert.pem",
		"-o", "build/test",
		"create",
		"build/test/kdm_cli_specify_cert"
	};

	vector<string> output;
	auto error = run(args, output);
	BOOST_CHECK(!error);

	BOOST_CHECK(output.empty());
	BOOST_CHECK(boost::filesystem::exists(kdm_filename));
}


BOOST_AUTO_TEST_CASE(kdm_cli_time)
{
	ConfigRestorer cr;

	setup_test_config();

	boost::filesystem::path kdm_filename = "build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV_Deans_Screens_Screen_2.xml";

	boost::system::error_code ec;
	boost::filesystem::remove(kdm_filename, ec);

	dcp::LocalTime now;
	now.add_days(2);

	vector<string> args = {
		"kdm_cli",
		"--verbose",
		"--valid-from", now.as_string(),
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


BOOST_AUTO_TEST_CASE(kdm_cli_add_dkdm)
{
	ConfigRestorer cr;

	setup_test_config();

	BOOST_CHECK_EQUAL(Config::instance()->dkdms()->children().size(), 0U);

	vector<string> args = {
		"kdm_cli",
		"add-dkdm",
		"test/data/dkdm.xml"
	};

	vector<string> output;
	auto error = run(args, output);
	BOOST_CHECK(!error);

	auto dkdms = Config::instance()->dkdms()->children();
	BOOST_CHECK_EQUAL(dkdms.size(), 1U);
	auto dkdm = dynamic_pointer_cast<DKDM>(dkdms.front());
	BOOST_CHECK(dkdm);
	BOOST_CHECK_EQUAL(dkdm->dkdm().as_xml(), dcp::file_to_string("test/data/dkdm.xml"));
}

