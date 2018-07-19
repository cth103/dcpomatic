/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup specific
 */

#include "lib/film.h"
#include "lib/text_caption_file_content.h"
#include "lib/dcp_content_type.h"
#include "lib/font.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <list>

using std::string;
using std::list;
using boost::shared_ptr;

/** Make a very short DCP with a single subtitle from .srt with no specified fonts */
BOOST_AUTO_TEST_CASE (srt_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_audio_channels (6);
	film->set_interop (false);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->make_dcp ();
	wait_for_jobs ();

	/* Should be blank video with a subtitle MXF */
	check_dcp ("test/data/srt_subtitle_test", film->dir (film->dcp_name ()));
}

/** Same again but with a `font' specified */
BOOST_AUTO_TEST_CASE (srt_subtitle_test2)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_audio_channels (6);
	film->set_interop (false);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	/* Use test/data/subrip2.srt as if it were a font file  */
	content->subtitle->fonts().front()->set_file (FontFiles::NORMAL, "test/data/subrip2.srt");

	film->make_dcp ();
	wait_for_jobs ();

	/* Should be blank video with a subtitle MXF */
	check_dcp ("test/data/srt_subtitle_test2", film->dir (film->dcp_name ()));
}

static void
check_subtitle_file (shared_ptr<Film> film, boost::filesystem::path ref)
{
	/* Find the subtitle file and check it */
	list<string> ignore;
	ignore.push_back ("SubtitleID");
	check_xml (subtitle_file(film), ref, ignore);
}

/** Make another DCP with a longer .srt file */
BOOST_AUTO_TEST_CASE (srt_subtitle_test3)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test3");

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	film->set_audio_channels (6);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, private_data / "Ankoemmling_short.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);

	film->make_dcp ();
	wait_for_jobs ();

	check_subtitle_file (film, private_data / "Ankoemmling_short.xml");
}

/** Build a small DCP with no picture and a single subtitle overlaid onto it */
BOOST_AUTO_TEST_CASE (srt_subtitle_test4)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test4");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (false);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/subrip2.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	/* Should be blank video with MXF subtitles */
	check_dcp ("test/data/xml_subtitle_test", film->dir (film->dcp_name ()));
}

/** Check the subtitle XML when there are two subtitle files in the project */
BOOST_AUTO_TEST_CASE (srt_subtitle_test5)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test5");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	film->set_sequence (false);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/subrip2.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->examine_and_add_content (content);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	content->set_position (DCPTime (0));
	film->make_dcp ();
	wait_for_jobs ();
	film->write_metadata ();

	check_dcp ("test/data/xml_subtitle_test2", film->dir (film->dcp_name ()));
}

BOOST_AUTO_TEST_CASE (srt_subtitle_test6)
{
	shared_ptr<Film> film = new_test_film2 ("srt_subtitle_test6");
	film->set_interop (false);
	shared_ptr<TextCaptionFileContent> content (new TextCaptionFileContent (film, "test/data/frames.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
	film->write_metadata ();

	check_dcp ("test/data/srt_subtitle_test6", film->dir(film->dcp_name()));
}

#if 0
/* XXX: this is disabled; there is some difference in font rendering
   between the test machine and others.
*/

/** Test rendering of a SubRip subtitle */
BOOST_AUTO_TEST_CASE (srt_subtitle_test4)
{
	shared_ptr<Film> film = new_test_film ("subrip_render_test");
	shared_ptr<TextCaptionFile> content (new TextCaptionFile (film, "test/data/subrip.srt"));
	content->examine (shared_ptr<Job> (), true);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds ((3 * 60) + 56.471));

	shared_ptr<SubRipDecoder> decoder (new SubRipDecoder (content));
	list<ContentTextSubtitle> cts = decoder->get_plain_texts (
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
