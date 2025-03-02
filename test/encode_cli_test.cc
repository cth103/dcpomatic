/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/encode_cli.h"
#include "lib/film.h"
#include "test.h"
#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <string>
#include <vector>


using std::cout;
using std::string;
using std::vector;
using boost::optional;


static
optional<string>
run(vector<string> const& args, vector<string>& output)
{
	vector<char*> argv(args.size() + 1);
	for (auto i = 0U; i < args.size(); ++i) {
		argv[i] = const_cast<char*>(args[i].c_str());
	}
	argv[args.size()] = nullptr;

	auto error = encode_cli(args.size(), argv.data(), [&output](string s) { output.push_back(s); }, []() { });
	if (error) {
		std::cout << *error << "\n";
	}

	return error;
}


static
bool
find_in_order(vector<string> const& output, vector<string> const& check)
{
	BOOST_REQUIRE(!check.empty());

	auto next = check.begin();
	for (auto line: output) {
		if (line.find(*next) != string::npos) {
			++next;
			if (next == check.end()) {
				return true;
			}
		}
	}

	return false;
}


BOOST_AUTO_TEST_CASE(basic_encode_cli_test)
{
	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film("basic_encode_cli_test", content);
	film->write_metadata();

	vector<string> output;
	run({ "cli", "build/test/basic_encode_cli_test" }, output);

	BOOST_CHECK(find_in_order(output, { "Making DCP for", "Examining content", "OK", "Transcoding DCP", "OK" }));
}
