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


#include "lib/cinema.h"
#include "lib/config.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <fstream>


using std::list;
using std::ofstream;
using std::make_shared;
using std::string;
using boost::optional;


static string
rewrite_bad_config (string filename, string extra_line)
{
	using namespace boost::filesystem;

	auto base = path("build/test/bad_config/2.18");
	auto file = base / filename;

	boost::system::error_code ec;
	remove (file, ec);

	boost::filesystem::create_directories (base);
	std::ofstream f (file.string().c_str());
	f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	  << "<Config>\n"
	  << "<Foo></Foo>\n"
	  << extra_line << "\n"
	  << "</Config>\n";
	f.close ();

	return dcp::file_to_string (file);
}


BOOST_AUTO_TEST_CASE (config_backup_test)
{
	ConfigRestorer cr;

	Config::override_path = "build/test/bad_config";
	Config::drop();
	boost::filesystem::remove_all ("build/test/bad_config");

	/* Write an invalid config file to config.xml */
	auto const first_write_xml = rewrite_bad_config("config.xml", "first write");

	/* Load the config; this should fail, causing the bad config to be copied to config.xml.1
	 * and a new config.xml created in its place.
	 */
	Config::instance();

	boost::filesystem::path const prefix = "build/test/bad_config/2.18";

	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.1"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.1") == first_write_xml);
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.2"));
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.3"));
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.4"));

	Config::drop();
	auto const second_write_xml = rewrite_bad_config("config.xml", "second write");
	Config::instance();

	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.1"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.1") == first_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.2"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.2") == second_write_xml);
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.3"));
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.4"));

	Config::drop();
	auto const third_write_xml = rewrite_bad_config("config.xml", "third write");
	Config::instance();

	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.1"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.1") == first_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.2"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.2") == second_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.3"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.3") == third_write_xml);
	BOOST_CHECK(!boost::filesystem::exists(prefix / "config.xml.4"));

	Config::drop();
	auto const fourth_write_xml = rewrite_bad_config("config.xml", "fourth write");
	Config::instance();

	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.1"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.1") == first_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.2"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.2") == second_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.3"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.3") == third_write_xml);
	BOOST_CHECK(boost::filesystem::exists(prefix / "config.xml.4"));
	BOOST_CHECK(dcp::file_to_string(prefix / "config.xml.4") == fourth_write_xml);
}


BOOST_AUTO_TEST_CASE (config_backup_with_link_test)
{
	using namespace boost::filesystem;

	ConfigRestorer cr;

	auto base = path("build/test/bad_config");
	auto version = base / "2.18";

	Config::override_path = base;
	Config::drop();

	boost::filesystem::remove_all (base);

	boost::filesystem::create_directories (version);
	std::ofstream f (path(version / "config.xml").string().c_str());
	f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	  << "<Config>\n"
	  << "<Link>" << path(version / "actual.xml").string() << "</Link>\n"
	  << "</Config>\n";
	f.close ();

	Config::drop ();
	/* Cause actual.xml to be backed up */
	rewrite_bad_config ("actual.xml", "first write");
	Config::instance ();

	/* Make sure actual.xml was backed up to the right place */
	BOOST_CHECK (boost::filesystem::exists(version / "actual.xml.1"));
}


BOOST_AUTO_TEST_CASE (config_write_utf8_test)
{
	ConfigRestorer cr;

	boost::filesystem::remove_all ("build/test/config.xml");
	boost::filesystem::copy_file ("test/data/utf8_config.xml", "build/test/config.xml");
	Config::override_path = "build/test";
	Config::drop ();
	Config::instance()->write();

	check_text_file ("test/data/utf8_config.xml", "build/test/config.xml");
}


/* 2.14 -> 2.18 */
BOOST_AUTO_TEST_CASE (config_upgrade_test1)
{
	ConfigRestorer cr;

	boost::filesystem::path dir = "build/test/config_upgrade_test";
	Config::override_path = dir;
	Config::drop ();
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	boost::filesystem::copy_file ("test/data/2.14.config.xml", dir / "config.xml");
	boost::filesystem::copy_file ("test/data/2.14.cinemas.xml", dir / "cinemas.xml");
	Config::instance();
	try {
		/* This will fail to write cinemas.xml since the link is to a non-existent directory */
		Config::instance()->write();
	} catch (...) {}

	check_xml (dir / "config.xml", "test/data/2.14.config.xml", {});
	check_xml (dir / "cinemas.xml", "test/data/2.14.cinemas.xml", {});
#ifdef DCPOMATIC_WINDOWS
	/* This file has the windows path for dkdm_recipients.xml (with backslashes) */
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.windows.xml", {});
#else
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.xml", {});
#endif
	/* cinemas.xml is not copied into 2.18 as its format has not changed */
	BOOST_REQUIRE (!boost::filesystem::exists(dir / "2.18" / "cinemas.xml"));
}


/* 2.16 -> 2.18 */
BOOST_AUTO_TEST_CASE (config_upgrade_test2)
{
	ConfigRestorer cr;

	boost::filesystem::path dir = "build/test/config_upgrade_test";
	Config::override_path = dir;
	Config::drop ();
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	boost::filesystem::copy_file("test/data/2.16.config.xml", dir / "config.xml");
	boost::filesystem::copy_file("test/data/2.14.cinemas.xml", dir / "cinemas.xml");
	Config::instance();
	try {
		/* This will fail to write cinemas.xml since the link is to a non-existent directory */
		Config::instance()->write();
	} catch (...) {}

	check_xml(dir / "config.xml", "test/data/2.16.config.xml", {});
	check_xml(dir / "cinemas.xml", "test/data/2.14.cinemas.xml", {});
#ifdef DCPOMATIC_WINDOWS
	/* This file has the windows path for dkdm_recipients.xml (with backslashes) */
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.windows.xml", {});
#else
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.xml", {});
#endif
	/* cinemas.xml is not copied into 2.18 as its format has not changed */
	BOOST_REQUIRE (!boost::filesystem::exists(dir / "2.18" / "cinemas.xml"));
}


BOOST_AUTO_TEST_CASE (config_keep_cinemas_if_making_new_config)
{
	ConfigRestorer cr;

	boost::filesystem::path dir = "build/test/config_keep_cinemas_if_making_new_config";
	Config::override_path = dir;
	Config::drop ();
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	Config::instance()->write();

	Config::instance()->add_cinema(make_shared<Cinema>("My Great Cinema", list<string>(), "", 0, 0));
	Config::instance()->write();

	boost::filesystem::copy_file (dir / "cinemas.xml", dir / "backup_for_test.xml");

	Config::drop ();
	boost::filesystem::remove (dir / "2.18" / "config.xml");
	Config::instance();

	check_text_file(dir / "backup_for_test.xml", dir / "cinemas.xml");
}


BOOST_AUTO_TEST_CASE(keep_config_if_cinemas_fail_to_load)
{
	ConfigRestorer cr;

	/* Make a new config */
	boost::filesystem::path dir = "build/test/keep_config_if_cinemas_fail_to_load";
	Config::override_path = dir;
	Config::drop();
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	Config::instance()->write();

	auto const cinemas = dir / "cinemas.xml";

	/* Back things up */
	boost::filesystem::copy_file(dir / "2.18" / "config.xml", dir / "config_backup_for_test.xml");
	boost::filesystem::copy_file(cinemas, dir / "cinemas_backup_for_test.xml");

	/* Corrupt the cinemas */
	Config::drop();
	std::ofstream corrupt(cinemas.string().c_str());
	corrupt << "foo\n";
	corrupt.close();
	Config::instance();

	/* We should have a new cinemas.xml and the old config.xml */
	check_text_file(dir / "2.18" / "config.xml", dir / "config_backup_for_test.xml");
	check_text_file(cinemas, dir / "cinemas_backup_for_test.xml");
}

