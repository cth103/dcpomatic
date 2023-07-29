/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/srt_subtitle_test.cc
 *  @brief Test writing DCPs with subtitles from .srt.
 *  @ingroup feature
 */


#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/ratio.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/subtitle_string.h>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <list>


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using namespace dcpomatic;


/** Make a very short DCP with a single subtitle from .srt with no specified fonts */
BOOST_AUTO_TEST_CASE (srt_subtitle_test)
{
	auto film = new_test_film ("srt_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_audio_channels (6);
	film->set_interop (false);
	auto content = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});


	/* Should be blank video with a subtitle MXF */
	check_dcp ("test/data/srt_subtitle_test", film->dir (film->dcp_name ()));
}


/** Same again but with a `font' specified */
BOOST_AUTO_TEST_CASE (srt_subtitle_test2)
{
	auto film = new_test_film ("srt_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_audio_channels (6);
	film->set_interop (false);
	auto content = make_shared<StringTextFileContent> ("test/data/subrip2.srt");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	/* Use test/data/subrip2.srt as if it were a font file  */
	content->only_text()->fonts().front()->set_file("test/data/subrip2.srt");

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* Should be blank video with a subtitle MXF; sound is irrelevant */
	check_dcp("test/data/srt_subtitle_test2", film->dir(film->dcp_name()), true);
}


static void
check_subtitle_file (shared_ptr<Film> film, boost::filesystem::path ref)
{
	/* Find the subtitle file and check it */
	check_xml (subtitle_file(film), ref, {"SubtitleID"});
}


/** Make another DCP with a longer .srt file */
BOOST_AUTO_TEST_CASE (srt_subtitle_test3)
{
	Cleanup cl;

	auto content = make_shared<StringTextFileContent>(TestPaths::private_data() / "Ankoemmling_short.srt");
	auto film = new_test_film2 ("srt_subtitle_test3", { content }, &cl);

	film->set_name ("frobozz");
	film->set_interop (true);
	film->set_audio_channels (6);

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);

	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_STANDARD});

	check_subtitle_file (film, TestPaths::private_data() / "Ankoemmling_short.xml");

	cl.run ();
}


/** Build a small DCP with no picture and a single subtitle overlaid onto it */
BOOST_AUTO_TEST_CASE (srt_subtitle_test4)
{
	auto film = new_test_film ("srt_subtitle_test4");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (false);
	auto content = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* Should be blank video with MXF subtitles; sound is irrelevant */
	check_dcp("test/data/xml_subtitle_test", film->dir(film->dcp_name()), true);
}


/** Check the subtitle XML when there are two subtitle files in the project */
BOOST_AUTO_TEST_CASE (srt_subtitle_test5)
{
	auto film = new_test_film ("srt_subtitle_test5");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	film->set_sequence (false);
	for (auto i = 0; i < 2; ++i) {
		auto content = make_shared<StringTextFileContent>("test/data/subrip2.srt");
		content->only_text()->set_use (true);
		content->only_text()->set_burn (false);
		film->examine_and_add_content (content);
		BOOST_REQUIRE (!wait_for_jobs());
		content->set_position (film, DCPTime());
	}
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_STANDARD});

	check_dcp ("test/data/xml_subtitle_test2", film->dir (film->dcp_name ()));
}


BOOST_AUTO_TEST_CASE (srt_subtitle_test6)
{
	auto content = make_shared<StringTextFileContent>("test/data/frames.srt");
	auto film = new_test_film2 ("srt_subtitle_test6", {content});
	film->set_interop (false);
	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
		});

	/* This test is concerned with the subtitles, so we'll ignore any
	 * differences in sound between the DCP and the reference to avoid test
	 * failures for unrelated reasons.
	 */
	check_dcp("test/data/srt_subtitle_test6", film->dir(film->dcp_name()), true);
}


/** Test a case where a & in srt ended up in the SMPTE subtitle as &amp;amp */
BOOST_AUTO_TEST_CASE(srt_subtitle_entity)
{
	std::ofstream srt("build/test/srt_subtitle_entity.srt");
	srt << "1\n";
	srt << "00:00:01,000 -> 00:00:10,000\n";
	srt << "Hello & world\n";
	srt.close();

	auto content = make_shared<StringTextFileContent>("build/test/srt_subtitle_entity.srt");
	auto film = new_test_film2("srt_subtitle_entity", { content });
	film->set_interop(false);
	content->only_text()->set_use(true);
	content->only_text()->set_burn(false);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
		});

	dcp::SMPTESubtitleAsset check(dcp_file(film, "sub_"));
	auto subs = check.subtitles();
	BOOST_REQUIRE_EQUAL(subs.size(), 1U);
	auto sub = std::dynamic_pointer_cast<const dcp::SubtitleString>(subs[0]);
	BOOST_REQUIRE(sub);
	/* libdcp::SubtitleAsset gets the text from the XML with get_content(), which
	 * resolves the 5 predefined entities & " < > ' so we shouldn't see any
	 * entity here.
	 */
	BOOST_CHECK_EQUAL(sub->text(), "Hello & world");

	/* It should be escaped in the raw XML though */
	BOOST_REQUIRE(static_cast<bool>(check.raw_xml()));
	BOOST_CHECK(check.raw_xml()->find("Hello &amp; world") != string::npos);
}


/** A control code in a .srt file should not make it into the XML */
BOOST_AUTO_TEST_CASE(srt_subtitle_control_code)
{
	std::ofstream srt("build/test/srt_subtitle_control_code.srt");
	srt << "1\n";
	srt << "00:00:01,000 -> 00:00:10,000\n";
	srt << "Hello \x0c world\n";
	srt.close();

	auto content = make_shared<StringTextFileContent>("build/test/srt_subtitle_control_code.srt");
	auto film = new_test_film2("srt_subtitle_control_code", { content });
	film->set_interop(false);
	content->only_text()->set_use(true);
	content->only_text()->set_burn(false);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});
}


#if 0
/* XXX: this is disabled; there is some difference in font rendering
   between the test machine and others.
*/

/** Test rendering of a SubRip subtitle */
BOOST_AUTO_TEST_CASE (srt_subtitle_test4)
{
	shared_ptr<Film> film = new_test_film ("subrip_render_test");
	shared_ptr<StringTextFile> content (new StringTextFile("test/data/subrip.srt"));
	content->examine (shared_ptr<Job> (), true);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds ((3 * 60) + 56.471));

	shared_ptr<SubRipDecoder> decoder (new SubRipDecoder (content));
	list<ContentStringText> cts = decoder->get_plain_texts (
		ContentTimePeriod (
			ContentTime::from_seconds (109), ContentTime::from_seconds (110)
			), false
		);
	BOOST_CHECK_EQUAL (cts.size(), 1);

	PositionImage image = render_text (cts.front().subs, dcp::Size (1998, 1080));
	write_image (image.image, "build/test/subrip_render_test.png");
	check_file ("build/test/subrip_render_test.png", "test/data/subrip_render_test.png");
}
#endif
