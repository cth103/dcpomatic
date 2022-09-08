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


#include "lib/kdm_cli.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::string;
using std::vector;


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

	char** argv = new char*[args.size()];
	for (auto i = 0U; i < args.size(); ++i) {
		argv[i] = const_cast<char*>(args[i].c_str());
	}

	boost::filesystem::path const kdm_filename = "build/test/KDM_Test_FTR-1_F-133_XX-XX_MOS_2K_20220109_SMPTE_OV__my_great_screen.xml";
	boost::system::error_code ec;
	boost::filesystem::remove(kdm_filename, ec);

	auto error = kdm_cli (args.size(), argv, [](string s) { std::cout << s << "\n"; });
	if (error) {
		std::cout << *error << "\n";
	}
	BOOST_CHECK (!error);

	BOOST_CHECK(boost::filesystem::exists(kdm_filename));

	delete[] argv;
}

