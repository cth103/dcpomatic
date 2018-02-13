/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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
	boost::filesystem::remove ("build/test/config.xml", ec);

	Config::test_path = "build/test";
	ofstream f ("build/test/config.xml");
	f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	  << "<Config>\n"
	  << "<Foo></Foo>\n"
	  << "</Config>\n";
	f.close ();
}


BOOST_AUTO_TEST_CASE (config_backup_test)
{
	Config::drop();

	boost::system::error_code ec;
	boost::filesystem::remove ("build/test/config.xml.1", ec);
	boost::filesystem::remove ("build/test/config.xml.2", ec);
	boost::filesystem::remove ("build/test/config.xml.3", ec);
	boost::filesystem::remove ("build/test/config.xml.4", ec);
	boost::filesystem::remove ("build/test/config.xml.5", ec);
	boost::filesystem::remove ("build/test/config.xml.5", ec);

	rewrite_bad_config();

	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.1"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.2"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.1"));
	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.2"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.1"));
	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.2"));
	BOOST_CHECK ( boost::filesystem::exists ("build/test/config.xml.3"));
	BOOST_CHECK (!boost::filesystem::exists ("build/test/config.xml.4"));

	Config::drop();
	rewrite_bad_config();
	Config::instance();

	BOOST_CHECK (boost::filesystem::exists ("build/test/config.xml.1"));
	BOOST_CHECK (boost::filesystem::exists ("build/test/config.xml.2"));
	BOOST_CHECK (boost::filesystem::exists ("build/test/config.xml.3"));
	BOOST_CHECK (boost::filesystem::exists ("build/test/config.xml.4"));

	/* This test has called Config::set_defaults(), so take us back
	   to the config that we want for our tests.
	*/
	setup_test_config ();
}
