/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

using std::list;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Test the logic which decides whether a DCP can be referenced or not */
BOOST_AUTO_TEST_CASE (vf_test1)
{
	shared_ptr<Film> film = new_test_film ("vf_test1");
	shared_ptr<DCPContent> dcp (new DCPContent (film, "test/data/reels_test2"));
	film->examine_and_add_content (dcp);
	wait_for_jobs ();

	/* Multi-reel DCP can't be referenced if we are using a single reel for the project */
	film->set_reel_type (REELTYPE_SINGLE);
	list<string> why_not;
	BOOST_CHECK (!dcp->can_reference_video(why_not));
	BOOST_CHECK (!dcp->can_reference_audio(why_not));
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	/* Multi-reel DCP can be referenced if we are using by-video-content */
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	BOOST_CHECK (dcp->can_reference_video(why_not));
	BOOST_CHECK (dcp->can_reference_audio(why_not));
	/* (but reels_test2 has no subtitles to reference) */
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	shared_ptr<FFmpegContent> other (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (other);
	wait_for_jobs ();

	/* Not possible if there is overlap */
	other->set_position (DCPTime (0));
	BOOST_CHECK (!dcp->can_reference_video(why_not));
	BOOST_CHECK (!dcp->can_reference_audio(why_not));
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));

	/* This should not be considered an overlap */
	other->set_position (dcp->end ());
	BOOST_CHECK (dcp->can_reference_video(why_not));
	BOOST_CHECK (dcp->can_reference_audio(why_not));
	/* (reels_test2 has no subtitles to reference) */
	BOOST_CHECK (!dcp->can_reference_subtitle(why_not));
}

/** Make a OV with video and audio and a VF referencing the OV and adding subs */
BOOST_AUTO_TEST_CASE (vf_test2)
{
	/* Make the OV */
	shared_ptr<Film> ov = new_test_film ("vf_test2_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	ov->set_name ("vf_test2_ov");
	shared_ptr<Content> video = content_factory (ov, "test/data/flat_red.png");
	ov->examine_and_add_content (video);
	wait_for_jobs ();
	video->video->set_length (24 * 5);
	shared_ptr<Content> audio = content_factory (ov, "test/data/white.wav");
	ov->examine_and_add_content (audio);
	wait_for_jobs ();
	ov->make_dcp ();
	wait_for_jobs ();

	/* Make the VF */
	shared_ptr<Film> vf = new_test_film ("vf_test2_vf");
	vf->set_name ("vf_test2_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	vf->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (content_factory (vf, ov->dir (ov->dcp_name ())));
	BOOST_REQUIRE (dcp);
	vf->examine_and_add_content (dcp);
	wait_for_jobs ();
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);
	shared_ptr<Content> sub = content_factory (vf, "test/data/subrip4.srt");
	vf->examine_and_add_content (sub);
	vf->make_dcp ();
	wait_for_jobs ();
	vf->write_metadata ();

	dcp::DCP ov_c (ov->dir (ov->dcp_name ()));
	ov_c.read ();
	BOOST_REQUIRE_EQUAL (ov_c.cpls().size(), 1);
	BOOST_REQUIRE_EQUAL (ov_c.cpls().front()->reels().size(), 1);
	BOOST_REQUIRE (ov_c.cpls().front()->reels().front()->main_picture());
	string const pic_id = ov_c.cpls().front()->reels().front()->main_picture()->id();
	BOOST_REQUIRE (ov_c.cpls().front()->reels().front()->main_sound());
	string const sound_id = ov_c.cpls().front()->reels().front()->main_sound()->id();
	BOOST_REQUIRE (!ov_c.cpls().front()->reels().front()->main_subtitle());

	dcp::DCP vf_c (vf->dir (vf->dcp_name ()));
	vf_c.read ();
	BOOST_REQUIRE_EQUAL (vf_c.cpls().size(), 1);
	BOOST_REQUIRE_EQUAL (vf_c.cpls().front()->reels().size(), 1);
	BOOST_REQUIRE (vf_c.cpls().front()->reels().front()->main_picture());
	BOOST_CHECK_EQUAL (vf_c.cpls().front()->reels().front()->main_picture()->id(), pic_id);
	BOOST_REQUIRE (vf_c.cpls().front()->reels().front()->main_sound());
	BOOST_CHECK_EQUAL (vf_c.cpls().front()->reels().front()->main_sound()->id(), sound_id);
	BOOST_REQUIRE (vf_c.cpls().front()->reels().front()->main_subtitle());
}
