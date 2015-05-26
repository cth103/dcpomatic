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

/** @file  test/recover_test.cc
 *  @brief Test recovery of a DCP transcode after a crash.
 */

#include <boost/test/unit_test.hpp>
#include <dcp/stereo_picture_mxf.h>
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "test.h"

using std::cout;
using std::string;
using boost::shared_ptr;

static void
note (dcp::NoteType t, string n)
{
	if (t == dcp::DCP_ERROR) {
		cout << n << "\n";
	}
}

BOOST_AUTO_TEST_CASE (recover_test)
{
	shared_ptr<Film> film = new_test_film ("recover_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("recover_test");
	film->set_three_d (true);

	shared_ptr<ImageContent> content (new ImageContent (film, "test/data/3d_test"));
	content->set_video_frame_type (VIDEO_FRAME_TYPE_3D_LEFT_RIGHT);
	film->examine_and_add_content (content);
	wait_for_jobs ();

	film->make_dcp ();
	wait_for_jobs ();

	boost::filesystem::path const video = "build/test/recover_test/video/185_2K_dfd7979910001f46bb36354c42701713_24_100000000_P_S_3D.mxf";

	boost::filesystem::copy_file (
		video,
		"build/test/recover_test/original.mxf"
		);
	
	boost::filesystem::resize_file (video, 2 * 1024 * 1024);

	film->make_dcp ();
	wait_for_jobs ();

	shared_ptr<dcp::StereoPictureMXF> A (new dcp::StereoPictureMXF ("build/test/recover_test/original.mxf"));
	shared_ptr<dcp::StereoPictureMXF> B (new dcp::StereoPictureMXF (video));

	dcp::EqualityOptions eq;
	eq.mxf_filenames_can_differ = true;
	BOOST_CHECK (A->equals (B, eq, boost::bind (&note, _1, _2)));
}
