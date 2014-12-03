/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/subrip_test.cc
 *  @brief Various tests of the subrip code.
 */

#include <boost/test/unit_test.hpp>
#include <dcp/subtitle_content.h>
#include "lib/subrip.h"
#include "lib/subrip_content.h"
#include "lib/subrip_decoder.h"
#include "lib/render_subtitles.h"
#include "test.h"

using std::list;
using std::vector;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Test rendering of a SubRip subtitle */
BOOST_AUTO_TEST_CASE (subrip_render_test)
{
	shared_ptr<Film> film = new_test_film ("subrip_render_test");
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip.srt"));
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
