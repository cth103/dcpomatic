/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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

#include "lib/create_cli.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>

using std::string;

static CreateCLI
run (string cmd)
{
	/* This approximates the logic which splits command lines up into argc/argv */

	boost::escaped_list_separator<char> els ("", " ", "\"\'");
	boost::tokenizer<boost::escaped_list_separator<char>> tok (cmd, els);

	std::vector<char*> argv(256);
	int argc = 0;

	for (auto i: tok) {
		argv[argc++] = strdup (i.c_str());
	}

	CreateCLI cc (argc, argv.data());

	for (int i = 0; i < argc; ++i) {
		free (argv[i]);
	}

	return cc;
}

BOOST_AUTO_TEST_CASE (create_cli_test)
{
	CreateCLI cc = run ("dcpomatic2_create --version");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK (cc.version);

	cc = run ("dcpomatic2_create --versionX");
	BOOST_REQUIRE (cc.error);
	BOOST_CHECK (boost::algorithm::starts_with(*cc.error, "dcpomatic2_create: unrecognised option '--versionX'"));

	cc = run ("dcpomatic2_create --help");
	BOOST_REQUIRE (cc.error);

	cc = run ("dcpomatic2_create -h");
	BOOST_REQUIRE (cc.error);

	cc = run ("dcpomatic2_create x --name frobozz --template bar");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.name, "frobozz");
	BOOST_REQUIRE (cc.template_name);
	BOOST_CHECK_EQUAL (*cc.template_name, "bar");

	cc = run ("dcpomatic2_create x --dcp-content-type FTR");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.dcp_content_type, DCPContentType::from_isdcf_name("FTR"));

	cc = run ("dcpomatic2_create x --dcp-frame-rate 30");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE (cc.dcp_frame_rate);
	BOOST_CHECK_EQUAL (*cc.dcp_frame_rate, 30);

	cc = run ("dcpomatic2_create x --container-ratio 185");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.container_ratio, Ratio::from_id("185"));

	cc = run ("dcpomatic2_create x --container-ratio XXX");
	BOOST_CHECK (cc.error);

	cc = run ("dcpomatic2_create x --still-length 42");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.still_length, 42);

	cc = run ("dcpomatic2_create x --standard SMPTE");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.standard, dcp::Standard::SMPTE);

	cc = run ("dcpomatic2_create x --standard interop");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL (cc.standard, dcp::Standard::INTEROP);

	cc = run ("dcpomatic2_create x --standard SMPTEX");
	BOOST_CHECK (cc.error);

	cc = run ("dcpomatic2_create x --config foo/bar");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE (cc.config_dir);
	BOOST_CHECK_EQUAL (*cc.config_dir, "foo/bar");

	cc = run ("dcpomatic2_create x --output fred/jim");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE (cc.output_dir);
	BOOST_CHECK_EQUAL (*cc.output_dir, "fred/jim");

	cc = run ("dcpomatic2_create x --outputX fred/jim");
	BOOST_CHECK (cc.error);

	cc = run ("dcpomatic2_create --config foo/bar --still-length 42 --output flaps fred jim sheila");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE (cc.config_dir);
	BOOST_CHECK_EQUAL (*cc.config_dir, "foo/bar");
	BOOST_CHECK_EQUAL (cc.still_length, 42);
	BOOST_REQUIRE (cc.output_dir);
	BOOST_CHECK_EQUAL (*cc.output_dir, "flaps");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 3U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "fred");
	BOOST_CHECK_EQUAL (cc.content[0].frame_type, VideoFrameType::TWO_D);
	BOOST_CHECK_EQUAL (cc.content[1].path, "jim");
	BOOST_CHECK_EQUAL (cc.content[1].frame_type, VideoFrameType::TWO_D);
	BOOST_CHECK_EQUAL (cc.content[2].path, "sheila");
	BOOST_CHECK_EQUAL (cc.content[2].frame_type, VideoFrameType::TWO_D);

	cc = run ("dcpomatic2_create --left-eye left.mp4 --right-eye right.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 2U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "left.mp4");
	BOOST_CHECK_EQUAL (cc.content[0].frame_type, VideoFrameType::THREE_D_LEFT);
	BOOST_CHECK_EQUAL (cc.content[1].path, "right.mp4");
	BOOST_CHECK_EQUAL (cc.content[1].frame_type, VideoFrameType::THREE_D_RIGHT);
	BOOST_CHECK_EQUAL (cc.fourk, false);

	cc = run ("dcpomatic2_create --fourk foo.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 1U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "foo.mp4");
	BOOST_CHECK_EQUAL (cc.fourk, true);
	BOOST_CHECK (!cc.error);

	cc = run ("dcpomatic2_create --j2k-bandwidth 120 foo.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 1U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "foo.mp4");
	BOOST_REQUIRE (cc.j2k_bandwidth);
	BOOST_CHECK_EQUAL (*cc.j2k_bandwidth, 120000000);
	BOOST_CHECK (!cc.error);

	cc = run ("dcpomatic2_create --channel L fred.wav --channel R jim.wav sheila.wav");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 3U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "fred.wav");
	BOOST_CHECK (cc.content[0].channel);
	BOOST_CHECK (*cc.content[0].channel == dcp::Channel::LEFT);
	BOOST_CHECK_EQUAL (cc.content[1].path, "jim.wav");
	BOOST_CHECK (cc.content[1].channel);
	BOOST_CHECK (*cc.content[1].channel == dcp::Channel::RIGHT);
	BOOST_CHECK_EQUAL (cc.content[2].path, "sheila.wav");
	BOOST_CHECK (!cc.content[2].channel);

	cc = run ("dcpomatic2_create --channel foo fred.wav");
	BOOST_REQUIRE (cc.error);
	BOOST_CHECK (boost::algorithm::starts_with(*cc.error, "dcpomatic2_create: foo is not valid for --channel"));

	cc = run ("dcpomatic2_create fred.wav --gain -6 jim.wav --gain 2 sheila.wav");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 3U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "fred.wav");
	BOOST_CHECK (!cc.content[0].gain);
	BOOST_CHECK_EQUAL (cc.content[1].path, "jim.wav");
	BOOST_CHECK_CLOSE (*cc.content[1].gain, -6, 0.001);
	BOOST_CHECK_EQUAL (cc.content[2].path, "sheila.wav");
	BOOST_CHECK_CLOSE (*cc.content[2].gain, 2, 0.001);
}
