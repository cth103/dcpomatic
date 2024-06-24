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
#include "lib/cinema_list.h"
#include "lib/config.h"
#include "lib/dkdm_recipient.h"
#include "lib/dkdm_recipient_list.h"
#include "lib/unzipper.h"
#include "lib/zipper.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <fstream>


using std::make_shared;
using std::ofstream;
using std::string;
using std::vector;
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
	ConfigRestorer cr("build/test/bad_config");
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

	auto base = path("build/test/bad_config");
	auto version = base / "2.18";

	ConfigRestorer cr(base);

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
	ConfigRestorer cr("build/test");

	boost::filesystem::remove_all ("build/test/config.xml");
	boost::filesystem::copy_file ("test/data/utf8_config.xml", "build/test/config.xml");
	Config::instance()->write();

	check_text_file ("test/data/utf8_config.xml", "build/test/config.xml");
}


/* 2.14 -> 2.18 */
BOOST_AUTO_TEST_CASE (config_upgrade_test1)
{
	boost::filesystem::path dir = "build/test/config_upgrade_test1";
	ConfigRestorer cr(dir);

	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	boost::filesystem::copy_file ("test/data/2.14.config.xml", dir / "config.xml");
	boost::filesystem::copy_file ("test/data/2.14.cinemas.xml", dir / "cinemas.xml");
	try {
		/* This will fail to read cinemas.xml since the link is to a non-existent directory */
		Config::instance();
	} catch (...) {}

	Config::instance()->write();

	check_xml (dir / "config.xml", "test/data/2.14.config.xml", {});
	check_xml (dir / "cinemas.xml", "test/data/2.14.cinemas.xml", {});
#ifdef DCPOMATIC_WINDOWS
	/* This file has the windows path for dkdm_recipients.xml (with backslashes) */
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.windows.sqlite.xml", {});
#else
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.sqlite.xml", {});
#endif
	/* cinemas.xml is not copied into 2.18 as its format has not changed */
	BOOST_REQUIRE (!boost::filesystem::exists(dir / "2.18" / "cinemas.xml"));
}


/* 2.16 -> 2.18 */
BOOST_AUTO_TEST_CASE (config_upgrade_test2)
{
	boost::filesystem::path dir = "build/test/config_upgrade_test2";
	ConfigRestorer cr(dir);
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

#ifdef DCPOMATIC_WINDOWS
	boost::filesystem::copy_file("test/data/2.16.config.windows.xml", dir / "config.xml");
#else
	boost::filesystem::copy_file("test/data/2.16.config.xml", dir / "config.xml");
#endif
	boost::filesystem::copy_file("test/data/2.14.cinemas.xml", dir / "cinemas.xml");
	try {
		/* This will fail to read cinemas.xml since the link is to a non-existent directory */
		Config::instance();
	} catch (...) {}

	Config::instance()->write();

	check_xml(dir / "cinemas.xml", "test/data/2.14.cinemas.xml", {});
#ifdef DCPOMATIC_WINDOWS
	/* This file has the windows path for dkdm_recipients.xml (with backslashes) */
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.windows.xml", {});
	check_xml(dir / "config.xml", "test/data/2.16.config.windows.xml", {});
#else
	check_xml(dir / "2.18" / "config.xml", "test/data/2.18.config.xml", {});
	check_xml(dir / "config.xml", "test/data/2.16.config.xml", {});
#endif
	/* cinemas.xml is not copied into 2.18 as its format has not changed */
	BOOST_REQUIRE (!boost::filesystem::exists(dir / "2.18" / "cinemas.xml"));
}


BOOST_AUTO_TEST_CASE (config_keep_cinemas_if_making_new_config)
{
	boost::filesystem::path dir = "build/test/config_keep_cinemas_if_making_new_config";
	ConfigRestorer cr(dir);
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	Config::instance()->write();

	CinemaList cinemas;
	cinemas.add_cinema({"My Great Cinema", {}, "", dcp::UTCOffset()});

	boost::filesystem::copy_file(dir / "cinemas.sqlite3", dir / "backup_for_test.sqlite3");

	Config::drop ();
	boost::filesystem::remove (dir / "2.18" / "config.xml");
	Config::instance();

	check_file(dir / "backup_for_test.sqlite3", dir / "cinemas.sqlite3");
}


BOOST_AUTO_TEST_CASE(keep_config_if_cinemas_fail_to_load)
{
	/* Make a new config */
	boost::filesystem::path dir = "build/test/keep_config_if_cinemas_fail_to_load";
	ConfigRestorer cr(dir);
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	Config::instance()->write();

	CinemaList cinema_list;
	cinema_list.add_cinema(Cinema("Foo", {}, "Bar", dcp::UTCOffset()));

	auto const cinemas = dir / "cinemas.sqlite3";

	/* Back things up */
	boost::filesystem::copy_file(dir / "2.18" / "config.xml", dir / "config_backup_for_test.xml");
	boost::filesystem::copy_file(cinemas, dir / "cinemas_backup_for_test.sqlite3");

	/* Corrupt the cinemas */
	Config::drop();
	std::ofstream corrupt(cinemas.string().c_str());
	corrupt << "foo\n";
	corrupt.close();
	Config::instance();

	/* We should have the old config.xml */
	check_text_file(dir / "2.18" / "config.xml", dir / "config_backup_for_test.xml");
}


BOOST_AUTO_TEST_CASE(read_cinemas_xml_and_write_sqlite)
{
	/* Set up a config with an XML cinemas file */
	boost::filesystem::path dir = "build/test/read_cinemas_xml_and_write_sqlite";
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	boost::filesystem::create_directories(dir / "2.18");

	boost::filesystem::copy_file("test/data/cinemas.xml", dir / "cinemas.xml");
	boost::filesystem::copy_file("test/data/2.18.config.xml", dir / "2.18" / "config.xml");
	{
		Editor editor(dir / "2.18" / "config.xml");
		editor.replace(
			"/home/realldoesnt/exist/this/path/is/nonsense.sqlite3",
			boost::filesystem::canonical(dir / "cinemas.xml").string()
			);
	}

	ConfigRestorer cr(dir);

	/* This should make a sqlite3 file containing the recipients from cinemas.xml */
	Config::instance();

	{
		CinemaList test(dir / "cinemas.sqlite3");

		/* The detailed creation of sqlite3 from XML is tested in cinema_list_test.cc */
		auto cinemas = test.cinemas();
		BOOST_REQUIRE_EQUAL(cinemas.size(), 3U);
		BOOST_CHECK_EQUAL(cinemas[0].second.name, "Great");
		BOOST_CHECK_EQUAL(cinemas[1].second.name, "classy joint");
		BOOST_CHECK_EQUAL(cinemas[2].second.name, "stinking dump");

		/* Add another recipient to the sqlite */
		test.add_cinema({"The ol' 1-seater", {}, "Quiet but lonely", dcp::UTCOffset()});
	}

	/* Reload the config; the old XML should not clobber the new sqlite3 */
	Config::drop();
	Config::instance();

	{
		CinemaList test(dir / "cinemas.sqlite3");

		auto cinemas = test.cinemas();
		BOOST_REQUIRE_EQUAL(cinemas.size(), 4U);
		BOOST_CHECK_EQUAL(cinemas[0].second.name, "Great");
		BOOST_CHECK_EQUAL(cinemas[1].second.name, "The ol' 1-seater");
		BOOST_CHECK_EQUAL(cinemas[2].second.name, "classy joint");
		BOOST_CHECK_EQUAL(cinemas[3].second.name, "stinking dump");
	}
}


BOOST_AUTO_TEST_CASE(read_dkdm_recipients_xml_and_write_sqlite)
{
	/* Set up a config with an XML cinemas file */
	boost::filesystem::path dir = "build/test/read_dkdm_recipients_xml_and_write_sqlite";
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	boost::filesystem::create_directories(dir / "2.18");

	boost::filesystem::copy_file("test/data/dkdm_recipients.xml", dir / "dkdm_recipients.xml");
	boost::filesystem::copy_file("test/data/2.18.config.xml", dir / "2.18" / "config.xml");
	{
		Editor editor(dir / "2.18" / "config.xml");
		editor.replace(
			"build/test/config_upgrade_test/dkdm_recipients.xml",
			boost::filesystem::canonical(dir / "dkdm_recipients.xml").string()
			);
	}

	ConfigRestorer cr(dir);

	/* This should make a sqlite3 file containing the recipients from dkdm_recipients.xml */
	Config::instance();

	{
		DKDMRecipientList test(dir / "dkdm_recipients.sqlite3");

		/* The detailed creation of sqlite3 from XML is tested in dkdm_recipient_list_test.cc */
		auto recipients = test.dkdm_recipients();
		BOOST_REQUIRE_EQUAL(recipients.size(), 2U);
		BOOST_CHECK_EQUAL(recipients[0].second.name, "Bob's Epics");
		BOOST_CHECK_EQUAL(recipients[1].second.name, "Sharon's Shorts");

		/* Add another recipient to the sqlite */
		test.add_dkdm_recipient({"Carl's Classics", "Oldies but goodies", {}, {}});
	}

	/* Reload the config; the old XML should not clobber the new sqlite3 */
	Config::drop();
	Config::instance();

	{
		DKDMRecipientList test(dir / "dkdm_recipients.sqlite3");

		auto recipients = test.dkdm_recipients();
		BOOST_REQUIRE_EQUAL(recipients.size(), 3U);
		BOOST_CHECK_EQUAL(recipients[0].second.name, "Bob's Epics");
		BOOST_CHECK_EQUAL(recipients[1].second.name, "Carl's Classics");
		BOOST_CHECK_EQUAL(recipients[2].second.name, "Sharon's Shorts");
	}
}


BOOST_AUTO_TEST_CASE(save_config_as_zip_test)
{
	boost::filesystem::path const dir = "build/test/save_config_as_zip_test";
	ConfigRestorer cr(dir);
	boost::system::error_code ec;
	boost::filesystem::remove_all(dir, ec);
	boost::filesystem::create_directories(dir);
	boost::filesystem::copy_file("test/data/2.18.config.xml", dir / "config.xml");

	Config::instance()->set_cinemas_file(dir / "cinemas.sqlite3");
	Config::instance()->set_dkdm_recipients_file(dir / "dkdm_recipients.sqlite3");

	CinemaList cinemas;
	cinemas.add_cinema({"My Great Cinema", {}, "", dcp::UTCOffset()});
	DKDMRecipientList recipients;
	recipients.add_dkdm_recipient({"Carl's Classics", "Oldies but goodies", {}, {}});

	boost::filesystem::path const zip = "build/test/save.zip";
	boost::filesystem::remove(zip, ec);
	save_all_config_as_zip(zip);
	Unzipper unzipper(zip);

	BOOST_CHECK(unzipper.contains("config.xml"));
	BOOST_CHECK(unzipper.contains("cinemas.sqlite3"));
	BOOST_CHECK(unzipper.contains("dkdm_recipients.sqlite3"));
}


/** Load a config ZIP file, which contains an XML cinemas file, and ask to overwrite
 *  the existing cinemas file that we had.
 */
BOOST_AUTO_TEST_CASE(load_config_from_zip_with_only_xml_current)
{
	ConfigRestorer cr;

	auto cinemas_file = Config::instance()->cinemas_file();

	boost::filesystem::path const zip = "build/test/load.zip";
	boost::system::error_code ec;
	boost::filesystem::remove(zip, ec);

	Zipper zipper(zip);
	zipper.add(
		"config.xml",
		boost::algorithm::replace_all_copy(
			dcp::file_to_string("test/data/2.18.config.xml"),
			"/home/realldoesnt/exist/this/path/is/nonsense.xml",
			""
			)
		);

	zipper.add("cinemas.xml", dcp::file_to_string("test/data/cinemas.xml"));
	zipper.close();

	Config::instance()->load_from_zip(zip, Config::CinemasAction::WRITE_TO_CURRENT_PATH);

	CinemaList cinema_list(cinemas_file);
	auto cinemas = cinema_list.cinemas();
	BOOST_REQUIRE_EQUAL(cinemas.size(), 3U);
	BOOST_CHECK_EQUAL(cinemas[0].second.name, "Great");
	BOOST_CHECK_EQUAL(cinemas[1].second.name, "classy joint");
	BOOST_CHECK_EQUAL(cinemas[2].second.name, "stinking dump");
}


/** Load a config ZIP file, which contains an XML cinemas file, and ask to write it to
 *  the location specified by the zipped config.xml.
 */
BOOST_AUTO_TEST_CASE(load_config_from_zip_with_only_xml_zip)
{
	ConfigRestorer cr;

	boost::filesystem::path const zip = "build/test/load.zip";
	boost::system::error_code ec;
	boost::filesystem::remove(zip, ec);

	Zipper zipper(zip);
	zipper.add(
		"config.xml",
		boost::algorithm::replace_all_copy(
			dcp::file_to_string("test/data/2.18.config.xml"),
			"/home/realldoesnt/exist/this/path/is/nonsense.sqlite3",
			"build/test/hide/it/here/cinemas.sqlite3"
			)
		);

	zipper.add("cinemas.xml", dcp::file_to_string("test/data/cinemas.xml"));
	zipper.close();

	Config::instance()->load_from_zip(zip, Config::CinemasAction::WRITE_TO_PATH_IN_ZIPPED_CONFIG);

	CinemaList cinema_list("build/test/hide/it/here/cinemas.sqlite3");
	auto cinemas = cinema_list.cinemas();
	BOOST_REQUIRE_EQUAL(cinemas.size(), 3U);
	BOOST_CHECK_EQUAL(cinemas[0].second.name, "Great");
	BOOST_CHECK_EQUAL(cinemas[1].second.name, "classy joint");
	BOOST_CHECK_EQUAL(cinemas[2].second.name, "stinking dump");
}


/** Load a config ZIP file, which contains an XML cinemas file, and ask to ignore it */
BOOST_AUTO_TEST_CASE(load_config_from_zip_with_only_xml_ignore)
{
	ConfigRestorer cr;

	CinemaList cinema_list("build/test/hide/it/here/cinemas.sqlite3");
	cinema_list.add_cinema(Cinema("Foo", {}, "Bar", dcp::UTCOffset()));

	boost::filesystem::path const zip = "build/test/load.zip";
	boost::system::error_code ec;
	boost::filesystem::remove(zip, ec);

	Zipper zipper(zip);
	zipper.add(
		"config.xml",
		boost::algorithm::replace_all_copy(
			dcp::file_to_string("test/data/2.18.config.xml"),
			"/home/realldoesnt/exist/this/path/is/nonsense.xml",
			"build/test/hide/it/here/cinemas.sqlite3"
			)
		);

	zipper.add("cinemas.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Cinemas/>");
	zipper.close();

	Config::instance()->load_from_zip(zip, Config::CinemasAction::IGNORE);

	auto cinemas = cinema_list.cinemas();
	BOOST_CHECK(!cinemas.empty());
}


BOOST_AUTO_TEST_CASE(use_sqlite_if_present)
{
	/* Set up a config with an XML cinemas file */
	boost::filesystem::path dir = "build/test/read_cinemas_xml_and_write_sqlite";
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	boost::filesystem::create_directories(dir / "2.18");

	boost::filesystem::copy_file("test/data/cinemas.xml", dir / "cinemas.xml");
	boost::filesystem::copy_file("test/data/2.18.config.xml", dir / "2.18" / "config.xml");
	{
		Editor editor(dir / "2.18" / "config.xml");
		editor.replace(
			"/home/realldoesnt/exist/this/path/is/nonsense.sqlite3",
			boost::filesystem::canonical(dir / "cinemas.xml").string()
			);
	}

	ConfigRestorer cr(dir);

	/* This should make a sqlite3 file containing the recipients from cinemas.xml.
	 * But it won't write config.xml, so config.xml will still point to cinemas.xml.
	 * This also happens in real life - but I'm not sure how (perhaps just when DoM is
	 * loaded but doesn't save the config, and then another tool is loaded).
	 */
	Config::instance();

	BOOST_CHECK(boost::filesystem::exists(dir / "cinemas.sqlite3"));

	Config::drop();

	BOOST_CHECK(Config::instance()->cinemas_file() == boost::filesystem::canonical(dir / "cinemas.sqlite3"));
}



