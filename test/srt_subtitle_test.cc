/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  test/subtitle_write_test.cc
 *  @brief Test writing DCPs with XML subtitles.
 */

#include "lib/film.h"
#include "lib/text_subtitle_content.h"
#include "lib/dcp_content_type.h"
#include "lib/font.h"
#include "lib/ratio.h"
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
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->set_use_subtitles (true);
	content->set_burn_subtitles (false);
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
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->set_use_subtitles (true);
	content->set_burn_subtitles (false);
	/* Use test/data/subrip2.srt as if it were a font file  */
	content->fonts().front()->set_file (FontFiles::NORMAL, "test/data/subrip2.srt");

	film->make_dcp ();
	wait_for_jobs ();

	/* Should be blank video with a subtitle MXF */
	check_dcp ("test/data/srt_subtitle_test2", film->dir (film->dcp_name ()));
}

/** Make another DCP with a longer .srt file */
BOOST_AUTO_TEST_CASE (srt_subtitle_test3)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test3");

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, private_data / "Ankoemmling.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->set_use_subtitles (true);
	content->set_burn_subtitles (false);

	film->make_dcp ();
	wait_for_jobs ();

	/* Find the subtitle file and check it */
	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (film->directory() / film->dcp_name (false));
		i != boost::filesystem::directory_iterator ();
		++i) {

		if (boost::filesystem::is_directory (i->path ())) {
			for (
				boost::filesystem::directory_iterator j = boost::filesystem::directory_iterator (i->path ());
				j != boost::filesystem::directory_iterator ();
				++j) {

				if (boost::algorithm::starts_with (j->path().leaf().string(), "sub_")) {
					list<string> ignore;
					ignore.push_back ("SubtitleID");
					check_xml (*j, private_data / "Ankoemmling.xml", ignore);
				}
			}
		}
	}
}

#if 0
/* XXX: this is disabled; there is some difference in font rendering
   between the test machine and others.
*/

/** Test rendering of a SubRip subtitle */
BOOST_AUTO_TEST_CASE (srt_subtitle_test4)
{
	shared_ptr<Film> film = new_test_film ("subrip_render_test");
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip.srt"));
	content->examine (shared_ptr<Job> (), true);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds ((3 * 60) + 56.471));

	shared_ptr<SubRipDecoder> decoder (new SubRipDecoder (content));
	list<ContentTextSubtitle> cts = decoder->get_text_subtitles (
		ContentTimePeriod (
			ContentTime::from_seconds (109), ContentTime::from_seconds (110)
			), false
		);
	BOOST_CHECK_EQUAL (cts.size(), 1);

	PositionImage image = render_subtitles (cts.front().subs, dcp::Size (1998, 1080));
	write_image (image.image, "build/test/subrip_render_test.png");
	check_file ("build/test/subrip_render_test.png", "test/data/subrip_render_test.png");
}
#endif
