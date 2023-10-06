/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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
#include "lib/content.h"
#include "lib/dcp_content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/map_cli.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
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

	auto error = map_cli(args.size(), argv.data(), [&output](string s) { output.push_back(s); });
	if (error) {
		std::cout << *error << "\n";
	}

	return error;
}


static
boost::filesystem::path
find_prefix(boost::filesystem::path dir, string prefix)
{
        auto iter = std::find_if(boost::filesystem::directory_iterator(dir), boost::filesystem::directory_iterator(), [prefix](boost::filesystem::path const& p) {
		return boost::starts_with(p.filename().string(), prefix);
        });

        BOOST_REQUIRE(iter != boost::filesystem::directory_iterator());
        return iter->path();
}


static
boost::filesystem::path
find_cpl(boost::filesystem::path dir)
{
        return find_prefix(dir, "cpl_");
}


/** Map a single DCP into a new DCP */
BOOST_AUTO_TEST_CASE(map_simple_dcp_copy)
{
	string const name = "map_simple_dcp_copy";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	BOOST_CHECK(boost::filesystem::is_regular_file(find_prefix(out, "j2c_")));
	BOOST_CHECK(boost::filesystem::is_regular_file(find_prefix(out, "pcm_")));
}


/** Map a single DCP into a new DCP, referring to the CPL by ID */
BOOST_AUTO_TEST_CASE(map_simple_dcp_copy_by_id)
{
	string const name = "map_simple_dcp_copy_by_id";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	dcp::CPL cpl(find_cpl(film->dir(film->dcp_name())));

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		cpl.id()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	BOOST_CHECK(boost::filesystem::is_regular_file(find_prefix(out, "j2c_")));
	BOOST_CHECK(boost::filesystem::is_regular_file(find_prefix(out, "pcm_")));
}


/** Map a single DCP into a new DCP using the symlink option */
BOOST_AUTO_TEST_CASE(map_simple_dcp_copy_with_symlinks)
{
	string const name = "map_simple_dcp_copy_with_symlinks";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		"-s",
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	/* We can't verify this DCP because the symlinks will make it fail
	 * (as it should be, I think).
	 */

	BOOST_CHECK(boost::filesystem::is_symlink(find_prefix(out, "j2c_")));
	BOOST_CHECK(boost::filesystem::is_symlink(find_prefix(out, "pcm_")));
}


/** Map a single DCP into a new DCP using the hardlink option */
BOOST_AUTO_TEST_CASE(map_simple_dcp_copy_with_hardlinks)
{
	string const name = "map_simple_dcp_copy_with_hardlinks";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		"-l",
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	/* The video file will have 3 links because DoM also makes a link into the video directory */
	BOOST_CHECK_EQUAL(boost::filesystem::hard_link_count(find_prefix(out, "j2c_")), 3U);
	BOOST_CHECK_EQUAL(boost::filesystem::hard_link_count(find_prefix(out, "pcm_")), 2U);
}


/** Map a single Interop DCP with subs into a new DCP */
BOOST_AUTO_TEST_CASE(map_simple_interop_dcp_with_subs)
{
	string const name = "map_simple_interop_dcp_with_subs";
	string const out = String::compose("build/test/%1_out", name);

	auto picture = content_factory("test/data/flat_red.png").front();
	auto subs = content_factory("test/data/15s.srt").front();
	auto film = new_test_film2(name + "_in", { picture, subs });
	film->set_interop(true);
	subs->only_text()->set_language(dcp::LanguageTag("de"));
	make_and_verify_dcp(film, {dcp::VerificationNote::Code::INVALID_STANDARD});

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {dcp::VerificationNote::Code::INVALID_STANDARD});
}


static
void
test_map_ov_vf_copy(vector<string> extra_args = {})
{
	string const name = "map_ov_vf_copy";
	string const out = String::compose("build/test/%1_out", name);

	auto ov_content = content_factory("test/data/flat_red.png");
	auto ov_film = new_test_film2(name + "_ov", ov_content);
	make_and_verify_dcp(ov_film);

	auto const ov_dir = ov_film->dir(ov_film->dcp_name());
	auto vf_ov = make_shared<DCPContent>(ov_dir);
	auto vf_sound = content_factory("test/data/sine_440.wav").front();
	auto vf_film = new_test_film2(name + "_vf", { vf_ov, vf_sound });
	vf_ov->set_reference_video(true);
	make_and_verify_dcp(vf_film, {dcp::VerificationNote::Code::EXTERNAL_ASSET}, false);

	auto const vf_dir = vf_film->dir(vf_film->dcp_name());

	vector<string> args = {
		"map_cli",
		"-o", out,
		"-d", ov_dir.string(),
		"-d", vf_dir.string(),
		find_cpl(vf_dir).string()
	};

	args.insert(std::end(args), std::begin(extra_args), std::end(extra_args));

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	check_file(find_file(out, "cpl_"), find_file(vf_dir, "cpl_"));
	check_file(find_file(out, "j2c_"), find_file(ov_dir, "j2c_"));
	check_file(find_file(out, "pcm_"), find_file(vf_dir, "pcm_"));
}


/** Map an OV and a VF into a single DCP */
BOOST_AUTO_TEST_CASE(map_ov_vf_copy)
{
	test_map_ov_vf_copy();
	test_map_ov_vf_copy({"-l"});
}


/** Map an OV and VF into a single DCP, where the VF refers to the OV's assets multiple times */
BOOST_AUTO_TEST_CASE(map_ov_vf_copy_multiple_reference)
{
	string const name = "map_ov_vf_copy_multiple_reference";
	string const out = String::compose("build/test/%1_out", name);

	auto ov_content = content_factory("test/data/flat_red.png");
	auto ov_film = new_test_film2(name + "_ov", ov_content);
	make_and_verify_dcp(ov_film);

	auto const ov_dir = ov_film->dir(ov_film->dcp_name());

	auto vf_ov1 = make_shared<DCPContent>(ov_dir);
	auto vf_ov2 = make_shared<DCPContent>(ov_dir);
	auto vf_sound = content_factory("test/data/sine_440.wav").front();
	auto vf_film = new_test_film2(name + "_vf", { vf_ov1, vf_ov2, vf_sound });
	vf_film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	vf_ov2->set_position(vf_film, vf_ov1->end(vf_film));
	vf_ov1->set_reference_video(true);
	vf_ov2->set_reference_video(true);
	make_and_verify_dcp(vf_film, {dcp::VerificationNote::Code::EXTERNAL_ASSET}, false);

	auto const vf_dir = vf_film->dir(vf_film->dcp_name());

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", ov_dir.string(),
		"-d", vf_dir.string(),
		"-l",
		find_cpl(vf_dir).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	check_file(find_file(out, "cpl_"), find_file(vf_dir, "cpl_"));
	check_file(find_file(out, "j2c_"), find_file(ov_dir, "j2c_"));
}


/** Map a single DCP into a new DCP using the rename option */
BOOST_AUTO_TEST_CASE(map_simple_dcp_copy_with_rename)
{
	ConfigRestorer cr;
	Config::instance()->set_dcp_asset_filename_format(dcp::NameFormat("hello%c"));
	string const name = "map_simple_dcp_copy_with_rename";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		"-r",
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, {});

	dcp::DCP out_dcp(out);
	out_dcp.read();

	BOOST_REQUIRE_EQUAL(out_dcp.cpls().size(), 1U);
	auto const cpl = out_dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 1U);
	auto const reel = cpl->reels()[0];
	BOOST_REQUIRE(reel->main_picture());
	BOOST_REQUIRE(reel->main_sound());
	auto const picture = reel->main_picture()->asset();
	BOOST_REQUIRE(picture);
	auto const sound = reel->main_sound()->asset();
	BOOST_REQUIRE(sound);

	BOOST_REQUIRE(picture->file());
	BOOST_CHECK(picture->file().get().filename() == picture->id() + ".mxf");

	BOOST_REQUIRE(sound->file());
	BOOST_CHECK(sound->file().get().filename() == sound->id() + ".mxf");
}


static
void
test_two_cpls_each_with_subs(string name, bool interop)
{
	string const out = String::compose("build/test/%1_out", name);

	vector<dcp::VerificationNote::Code> acceptable_errors;
	if (interop) {
		acceptable_errors.push_back(dcp::VerificationNote::Code::INVALID_STANDARD);
	} else {
		acceptable_errors.push_back(dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE);
		acceptable_errors.push_back(dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME);
	}

	shared_ptr<Film> films[2];
	for (auto i = 0; i < 2; ++i) {
		auto picture = content_factory("test/data/flat_red.png").front();
		auto subs = content_factory("test/data/15s.srt").front();
		films[i] = new_test_film2(String::compose("%1_%2_in", name, i), { picture, subs });
		films[i]->set_interop(interop);
		subs->only_text()->set_language(dcp::LanguageTag("de"));
		make_and_verify_dcp(films[i], acceptable_errors);
	}

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", films[0]->dir(films[0]->dcp_name()).string(),
		"-d", films[1]->dir(films[1]->dcp_name()).string(),
		find_cpl(films[0]->dir(films[0]->dcp_name())).string(),
		find_cpl(films[1]->dir(films[1]->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, acceptable_errors);
}


BOOST_AUTO_TEST_CASE(map_two_interop_cpls_each_with_subs)
{
	test_two_cpls_each_with_subs("map_two_interop_cpls_each_with_subs", true);
}


BOOST_AUTO_TEST_CASE(map_two_smpte_cpls_each_with_subs)
{
	test_two_cpls_each_with_subs("map_two_smpte_cpls_each_with_subs", false);
}


BOOST_AUTO_TEST_CASE(map_with_given_config)
{
	ConfigRestorer cr;

	string const name = "map_with_given_config";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		"--config", "test/data/map_with_given_config",
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);
	boost::filesystem::remove_all("test/data/map_with_given_config/2.18");

	Config::instance()->drop();
	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	/* It should be signed by the key in test/data/map_with_given_config, not the one in test/data/signer_key */
	BOOST_CHECK(dcp::file_to_string(find_file(out, "cpl_")).find("dnQualifier=\\+uOcNN2lPuxpxgd/5vNkkBER0GE=,CN=CS.dcpomatic.smpte-430-2.LEAF,OU=dcpomatic.com,O=dcpomatic.com") != std::string::npos);
}


BOOST_AUTO_TEST_CASE(map_multireel_interop_ov_and_vf_adding_ccaps)
{
	string const name = "map_multireel_interop_ov_and_vf_adding_ccaps";
	string const out = String::compose("build/test/%1_out", name);

	vector<shared_ptr<Content>> video = {
		content_factory("test/data/flat_red.png")[0],
		content_factory("test/data/flat_red.png")[0],
		content_factory("test/data/flat_red.png")[0]
	};

	auto ov = new_test_film2(name + "_ov", { video[0], video[1], video[2] });
	ov->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	ov->set_interop(true);
	make_and_verify_dcp(ov, { dcp::VerificationNote::Code::INVALID_STANDARD });

	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));

	vector<shared_ptr<Content>> ccap = {
		content_factory("test/data/short.srt")[0],
		content_factory("test/data/short.srt")[0],
		content_factory("test/data/short.srt")[0]
	};

	auto vf = new_test_film2(name + "_vf", { ov_dcp, ccap[0], ccap[1], ccap[2] });
	vf->set_interop(true);
	vf->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video(true);
	ov_dcp->set_reference_audio(true);
	for (auto i = 0; i < 3; ++i) {
		ccap[i]->text[0]->set_use(true);
		ccap[i]->text[0]->set_type(TextType::CLOSED_CAPTION);
	}
	make_and_verify_dcp(
		vf,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::EXTERNAL_ASSET
		});

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", ov->dir(ov->dcp_name()).string(),
		"-d", vf->dir(vf->dcp_name()).string(),
		find_cpl(vf->dir(vf->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, { dcp::VerificationNote::Code::INVALID_STANDARD });
}


BOOST_AUTO_TEST_CASE(map_uses_config_for_issuer_and_creator)
{
	ConfigRestorer cr;

	Config::instance()->set_dcp_issuer("ostrabagalous");
	Config::instance()->set_dcp_creator("Fred");

	string const name = "map_uses_config_for_issuer_and_creator";
	string const out = String::compose("build/test/%1_out", name);

	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2(name + "_in", content);
	make_and_verify_dcp(film);

	vector<string> const args = {
		"map_cli",
		"-o", out,
		"-d", film->dir(film->dcp_name()).string(),
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	cxml::Document assetmap("AssetMap");
	assetmap.read_file(film->dir(film->dcp_name()) / "ASSETMAP.xml");
	BOOST_CHECK(assetmap.string_child("Issuer") == "ostrabagalous");
	BOOST_CHECK(assetmap.string_child("Creator") == "Fred");

	cxml::Document pkl("PackingList");
	pkl.read_file(find_prefix(out, "pkl_"));
	BOOST_CHECK(pkl.string_child("Issuer") == "ostrabagalous");
	BOOST_CHECK(pkl.string_child("Creator") == "Fred");
}


BOOST_AUTO_TEST_CASE(map_handles_interop_png_subs)
{
	string const name = "map_handles_interop_png_subs";
	auto arrietty = content_factory(TestPaths::private_data() / "arrietty_JP-EN.mkv")[0];
	auto film = new_test_film2(name + "_input", { arrietty });
	film->set_interop(true);
	arrietty->set_trim_end(dcpomatic::ContentTime::from_seconds(110));
	arrietty->text[0]->set_use(true);
	arrietty->text[0]->set_language(dcp::LanguageTag("de"));
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_STANDARD
		});

	auto const out = boost::filesystem::path("build") / "test" / (name + "_output");

	vector<string> const args = {
		"map_cli",
		"-o", out.string(),
		"-d", film->dir(film->dcp_name()).string(),
		find_cpl(film->dir(film->dcp_name())).string()
	};

	boost::filesystem::remove_all(out);

	vector<string> output_messages;
	auto error = run(args, output_messages);
	BOOST_CHECK(!error);

	verify_dcp(out, { dcp::VerificationNote::Code::INVALID_STANDARD });
}

