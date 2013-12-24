/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

/** @file  test/seek_zero_test.cc
 *  @brief Test seek to zero with a raw FFmpegDecoder (without the player
 *  confusing things as it might in ffmpeg_seek_test).
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_decoder.h"
#include "test.h"

using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (seek_zero_test)
{
	shared_ptr<Film> film = new_test_film ("seek_zero_test");
	film->set_name ("seek_zero_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/count300bd48.m2ts"));
	content->set_ratio (Ratio::from_id ("185"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	FFmpegDecoder decoder (film, content, true, false);
	shared_ptr<Decoded> a = decoder.peek ();
	cout << a->content_time << "\n";
	decoder.seek (0, true);
	shared_ptr<Decoded> b = decoder.peek ();
	cout << b->content_time << "\n";

	/* a will be after no seek, and b after a seek to zero, which should
	   have the same effect.
	*/
	BOOST_CHECK_EQUAL (a->content_time, b->content_time);
}
