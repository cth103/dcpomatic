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


#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/content.h"
#include "lib/create_cli.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <fmt/format.h>
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
	boost::tokenizer<boost::escaped_list_separator<char> > tok (cmd, els);

	std::vector<char*> argv(256);
	int argc = 0;

	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i) {
		argv[argc++] = strdup (i->c_str());
	}

	CreateCLI cc (argc, argv.data());

	for (int i = 0; i < argc; ++i) {
		free (argv[i]);
	}

	return cc;
}

BOOST_AUTO_TEST_CASE (create_cli_test)
{
	string collected_error;
	auto error = [&collected_error](string s) {
		collected_error += s;
	};

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
	BOOST_CHECK(collected_error.empty());

	cc = run ("dcpomatic2_create x --name frobozz --template bar");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL(cc._name, "frobozz");
	BOOST_REQUIRE(cc._template_name);
	BOOST_CHECK_EQUAL(*cc._template_name, "bar");
	BOOST_CHECK(collected_error.empty());

	cc = run ("dcpomatic2_create x --dcp-content-type FTR");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL(cc._dcp_content_type, DCPContentType::from_isdcf_name("FTR"));

	cc = run ("dcpomatic2_create x --dcp-frame-rate 30");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE (cc.dcp_frame_rate);
	BOOST_CHECK_EQUAL (*cc.dcp_frame_rate, 30);

	cc = run ("dcpomatic2_create x --container-ratio 185");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK(cc._container_ratio == Ratio::from_id("185"));

	cc = run ("dcpomatic2_create x --container-ratio XXX");
	BOOST_CHECK (cc.error);

	cc = run ("dcpomatic2_create x --still-length 42");
	BOOST_CHECK (!cc.error);
	BOOST_CHECK_EQUAL(cc.still_length.get_value_or(0), 42);

	cc = run ("dcpomatic2_create x --standard SMPTE");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE(cc._standard);
	BOOST_CHECK_EQUAL(*cc._standard, dcp::Standard::SMPTE);

	cc = run ("dcpomatic2_create x --standard interop");
	BOOST_CHECK (!cc.error);
	BOOST_REQUIRE(cc._standard);
	BOOST_CHECK_EQUAL(*cc._standard, dcp::Standard::INTEROP);

	cc = run ("dcpomatic2_create x --standard SMPTEX");
	BOOST_CHECK (cc.error);

	cc = run("dcpomatic2_create x --no-encrypt");
	BOOST_CHECK(cc._no_encrypt);

	cc = run("dcpomatic2_create x --encrypt");
	BOOST_CHECK(cc._encrypt);

	cc = run("dcpomatic2_create x --no-encrypt --encrypt");
	BOOST_CHECK(cc.error);

	cc = run("dcpomatic2_create x --twod");
	BOOST_CHECK(cc._twod);

	cc = run("dcpomatic2_create x --threed");
	BOOST_CHECK(cc._threed);

	cc = run("dcpomatic2_create x --twod --threed");
	BOOST_CHECK(cc.error);

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
	BOOST_CHECK_EQUAL(cc.still_length.get_value_or(0), 42);
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
	BOOST_CHECK_EQUAL(cc._fourk, false);

	cc = run ("dcpomatic2_create --colourspace rec1886 test/data/flat_red.png");
	BOOST_REQUIRE_EQUAL(cc.content.size(), 1U);
	BOOST_CHECK_EQUAL(cc.content[0].colour_conversion.get_value_or(""), "rec1886");
	BOOST_CHECK(!cc.error);
	auto film = cc.make_film(error);
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_REQUIRE(static_cast<bool>(film->content()[0]->video->colour_conversion()));
	BOOST_REQUIRE(film->content()[0]->video->colour_conversion() == PresetColourConversion::from_id("rec1886").conversion);

	cc = run ("dcpomatic2_create --colourspace ostrobogulous foo.mp4");
	BOOST_CHECK_EQUAL(cc.error.get_value_or(""), "dcpomatic2_create: ostrobogulous is not a recognised colourspace");

	cc = run ("dcpomatic2_create --twok foo.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 1U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "foo.mp4");
	BOOST_CHECK_EQUAL(cc._twok, true);
	BOOST_CHECK (!cc.error);

	cc = run ("dcpomatic2_create --fourk foo.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 1U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "foo.mp4");
	BOOST_CHECK_EQUAL(cc._fourk, true);
	BOOST_CHECK (!cc.error);

	cc = run("dcpomatic2_create --auto-crop foo.mp4 bar.mp4 --auto-crop baz.mp4");
	BOOST_REQUIRE_EQUAL(cc.content.size(), 3U);
	BOOST_CHECK(cc.content[0].auto_crop);
	BOOST_CHECK(!cc.content[1].auto_crop);
	BOOST_CHECK(cc.content[2].auto_crop);

	cc = run("dcpomatic2_create --auto-crop foo.mp4 bar.mp4 --auto-crop baz.mp4");
	BOOST_REQUIRE_EQUAL(cc.content.size(), 3U);
	BOOST_CHECK(cc.content[0].auto_crop);
	BOOST_CHECK(!cc.content[1].auto_crop);
	BOOST_CHECK(cc.content[2].auto_crop);

	cc = run("dcpomatic2_create --auto-crop-threshold 42 --auto-crop foo.mp4 bar.mp4 --auto-crop baz.mp4");
	BOOST_REQUIRE_EQUAL(cc.content.size(), 3U);
	BOOST_CHECK(cc.content[0].auto_crop);
	BOOST_CHECK(!cc.content[1].auto_crop);
	BOOST_CHECK(cc.content[2].auto_crop);
	BOOST_CHECK_EQUAL(cc.auto_crop_threshold.get_value_or(0), 42);

	auto pillarbox = TestPaths::private_data() / "pillarbox.png";
	cc = run("dcpomatic2_create --auto-crop " + pillarbox.string());
	film = cc.make_film(error);
	BOOST_CHECK_EQUAL(film->content().size(), 1U);
	BOOST_CHECK(film->content()[0]->video->actual_crop() == Crop(113, 262, 0, 0));
	BOOST_CHECK_EQUAL(collected_error, fmt::format("Cropped {} to 113 left, 262 right, 0 top and 0 bottom", pillarbox.string()));
	collected_error = "";

	cc = run ("dcpomatic2_create --video-bit-rate 120 foo.mp4");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 1U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "foo.mp4");
	BOOST_REQUIRE(cc._video_bit_rate);
	BOOST_CHECK_EQUAL(*cc._video_bit_rate, 120000000);
	BOOST_CHECK (!cc.error);

	cc = run ("dcpomatic2_create --channel L test/data/L.wav --channel R test/data/R.wav test/data/Lfe.wav");
	BOOST_REQUIRE_EQUAL (cc.content.size(), 3U);
	BOOST_CHECK_EQUAL (cc.content[0].path, "test/data/L.wav");
	BOOST_CHECK (cc.content[0].channel);
	BOOST_CHECK (*cc.content[0].channel == dcp::Channel::LEFT);
	BOOST_CHECK_EQUAL (cc.content[1].path, "test/data/R.wav");
	BOOST_CHECK (cc.content[1].channel);
	BOOST_CHECK (*cc.content[1].channel == dcp::Channel::RIGHT);
	BOOST_CHECK_EQUAL (cc.content[2].path, "test/data/Lfe.wav");
	BOOST_CHECK (!cc.content[2].channel);
	film = cc.make_film(error);
	BOOST_CHECK_EQUAL(film->audio_channels(), 6);
	BOOST_CHECK(collected_error.empty());

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

	cc = run("dcpomatic2_create --cpl 123456-789-0 dcp");
	BOOST_REQUIRE_EQUAL(cc.content.size(), 1U);
	BOOST_CHECK_EQUAL(cc.content[0].path, "dcp");
	BOOST_REQUIRE(static_cast<bool>(cc.content[0].cpl));
	BOOST_CHECK_EQUAL(*cc.content[0].cpl, "123456-789-0");

	cc = run("dcpomatic2_create -s SMPTE sheila.wav");
	BOOST_CHECK(!cc.still_length);
	BOOST_CHECK(cc.error);

	cc = run("dcpomatic2_create --channel L fred.wav --channel R jim.wav --channel C sheila.wav --audio-channels 2");
	BOOST_REQUIRE(cc.error);
	BOOST_CHECK_EQUAL(*cc.error, "dcpomatic2_create: cannot map audio as requested with only 2 channels");

	cc = run("dcpomatic2_create --channel L fred.wav --channel R jim.wav --channel C sheila.wav --audio-channels 3");
	BOOST_REQUIRE(cc.error);
	BOOST_CHECK_EQUAL(*cc.error, "dcpomatic2_create: audio channel count must be even");

	cc = run("dcpomatic2_create --channel L test/data/L.wav --channel R test/data/R.wav --channel C test/data/C.wav");
	BOOST_CHECK(!cc.error);
	film = cc.make_film(error);
	BOOST_CHECK_EQUAL(film->audio_channels(), 6);
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create --channel L test/data/L.wav --channel R test/data/R.wav --channel HI test/data/sine_440.wav");
	BOOST_CHECK(!cc.error);
	film = cc.make_film(error);
	BOOST_CHECK_EQUAL(film->audio_channels(), 8);
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create --channel L test/data/L.wav --channel R test/data/R.wav --channel C test/data/C.wav --audio-channels 16");
	BOOST_CHECK(!cc.error);
	film = cc.make_film(error);
	BOOST_CHECK_EQUAL(film->audio_channels(), 16);
	BOOST_CHECK(collected_error.empty());

	 cc = run("dcpomatic2_create --channel L --fade-in 0.5 test/data/L.wav --channel R test/data/R.wav");
	BOOST_CHECK(!cc.error);
	film = cc.make_film(error);
	BOOST_REQUIRE_EQUAL(film->content().size(), 2U);
	BOOST_REQUIRE(film->content()[0]->audio);
	BOOST_REQUIRE(film->content()[1]->audio);
	BOOST_CHECK(film->content()[0]->audio->fade_in() == dcpomatic::ContentTime::from_seconds(0.5));
	BOOST_CHECK(film->content()[0]->audio->fade_out() == dcpomatic::ContentTime{});
	BOOST_CHECK(film->content()[1]->audio->fade_in() == dcpomatic::ContentTime{});
	BOOST_CHECK(film->content()[1]->audio->fade_out() == dcpomatic::ContentTime{});
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create --fade-out 0.25 test/data/L.wav --fade-in 1 test/data/red_24.mp4");
	BOOST_CHECK(!cc.error);
	film = cc.make_film(error);
	BOOST_REQUIRE_EQUAL(film->content().size(), 2U);
	BOOST_REQUIRE(film->content()[1]->audio);
	BOOST_CHECK(film->content()[1]->audio->fade_in() == dcpomatic::ContentTime{});
	BOOST_CHECK(film->content()[1]->audio->fade_out() == dcpomatic::ContentTime::from_seconds(0.25));
	BOOST_REQUIRE(film->content()[0]->video);
	BOOST_CHECK(film->content()[0]->video->fade_in() == 24);
	BOOST_CHECK(film->content()[0]->video->fade_out() == 0);
	BOOST_CHECK(collected_error.empty());
}


BOOST_AUTO_TEST_CASE(create_cli_template_test)
{
	ConfigRestorer cr("test/data");

	string collected_error;
	auto error = [&collected_error](string s) {
		collected_error += s;
	};

	auto cc = run("dcpomatic2_create test/data/flat_red.png");
	auto film = cc.make_film(error);
	BOOST_CHECK(!film->three_d());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template 2d");
	film = cc.make_film(error);
	BOOST_CHECK(!film->three_d());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template 2d --threed");
	film = cc.make_film(error);
	BOOST_CHECK(film->three_d());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template 3d");
	film = cc.make_film(error);
	BOOST_CHECK(film->three_d());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template 3d --twod");
	film = cc.make_film(error);
	BOOST_CHECK(!film->three_d());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png");
	film = cc.make_film(error);
	BOOST_CHECK(!film->encrypted());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template unencrypted");
	film = cc.make_film(error);
	BOOST_CHECK(!film->encrypted());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template unencrypted --encrypt");
	film = cc.make_film(error);
	BOOST_CHECK(film->encrypted());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template encrypted");
	film = cc.make_film(error);
	BOOST_CHECK(film->encrypted());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template encrypted --no-encrypt");
	film = cc.make_film(error);
	BOOST_CHECK(!film->encrypted());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png");
	film = cc.make_film(error);
	BOOST_CHECK(!film->interop());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template interop");
	film = cc.make_film(error);
	BOOST_CHECK(film->interop());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template interop --standard SMPTE");
	film = cc.make_film(error);
	BOOST_CHECK(!film->interop());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template smpte");
	film = cc.make_film(error);
	BOOST_CHECK(!film->interop());
	BOOST_CHECK(collected_error.empty());

	cc = run("dcpomatic2_create test/data/flat_red.png --template smpte --standard interop");
	film = cc.make_film(error);
	BOOST_CHECK(film->interop());
	BOOST_CHECK(collected_error.empty());
}
