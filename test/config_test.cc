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


#include "lib/config.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <fstream>


using std::ofstream;


static void
rewrite_bad_config ()
{
	boost::system::error_code ec;
	boost::filesystem::remove ("build/test/bad_config/2.16/config.xml", ec);

	Config::override_path = "build/test/bad_config";
	boost::filesystem::create_directories ("build/test/bad_config/2.16");
	ofstream f ("build/test/bad_config/2.16/config.xml");
	f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	  << "<Config>\n"
	  << "<Foo></Foo>\n"
	  << "</Config>\n";
	f.close ();
}


BOOST_AUTO_TEST_CASE (config_backup_test)
{
	Config::override_path = "build/test/bad_config";

	Config::drop();

	boost::filesystem::remove_all ("build/test/bad_config");

	rewrite_bad_config();

	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.1"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.2"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.1"));
	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.2"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.1"));
	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.2"));
	BOOST_CHECK ( boost::filesystem::exists("build/test/bad_config/2.16/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists("build/test/bad_config/2.16/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK (boost::filesystem::exists("build/test/bad_config/2.16/config.xml.1"));
	BOOST_CHECK (boost::filesystem::exists("build/test/bad_config/2.16/config.xml.2"));
	BOOST_CHECK (boost::filesystem::exists("build/test/bad_config/2.16/config.xml.3"));
	BOOST_CHECK (boost::filesystem::exists("build/test/bad_config/2.16/config.xml.4"));

	/* This test has called Config::set_defaults(), so take us back
	   to the config that we want for our tests.
	*/
	setup_test_config ();
}


BOOST_AUTO_TEST_CASE (config_write_utf8_test)
{
	boost::filesystem::remove_all ("build/test/config.xml");
	boost::filesystem::copy_file ("test/data/utf8_config.xml", "build/test/config.xml");
	Config::override_path = "build/test";
	Config::drop ();
	Config::instance()->write();

	check_text_file ("test/data/utf8_config.xml", "build/test/config.xml");

	/* This test has called Config::set_defaults(), so take us back
	   to the config that we want for our tests.
	*/
	setup_test_config ();
}


BOOST_AUTO_TEST_CASE (config_upgrade_test)
{
	boost::filesystem::path dir = "build/test/config_upgrade_test";
	Config::override_path = dir;
	Config::drop ();
	boost::filesystem::remove_all (dir);
	boost::filesystem::create_directories (dir);

	boost::filesystem::copy_file ("test/data/2.14.config.xml", dir / "config.xml");
	boost::filesystem::copy_file ("test/data/2.14.cinemas.xml", dir / "cinemas.xml");
	Config::instance();
	try {
		/* This will fail to write cinemas.xml since the link is to a non-existant directory */
		Config::instance()->write();
	} catch (...) {}

	check_xml (dir / "config.xml", "test/data/2.14.config.xml", {});
	check_xml (dir / "cinemas.xml", "test/data/2.14.cinemas.xml", {});
	check_xml (dir / "2.16" / "config.xml", "test/data/2.16.config.xml", {});
	/* cinemas.xml is not copied into 2.16 as its format has not changed */
	BOOST_REQUIRE (!boost::filesystem::exists(dir / "2.16" / "cinemas.xml"));

	setup_test_config();
}

