/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/isdcf_name_test.cc
 *  @brief Test creation of ISDCF names.
 *  @ingroup specific
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/image_content.h"
#include "lib/video_content.h"
#include "lib/audio_mapping.h"
#include "lib/ffmpeg_content.h"
#include "lib/audio_content.h"
#include "test.h"
#include <iostream>

using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (isdcf_name_test)
{
	shared_ptr<Film> film = new_test_film ("isdcf_name_test");

	/* A basic test */

	film->set_name ("My Nice Film");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->_isdcf_date = boost::gregorian::date (2014, boost::gregorian::Jul, 4);
	ISDCFMetadata m;
	m.content_version = 1;
	m.audio_language = "EN";
	m.subtitle_language = "XX";
	m.territory = "UK";
	m.rating = "PG";
	m.studio = "ST";
	m.facility = "FA";
	film->set_isdcf_metadata (m);
	film->set_interop (true);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilm_FTR-1_F_EN-XX_UK-PG_2K_ST_20140704_FA_IOP_OV");

	/* Test a long name and some different data */

	film->set_name ("My Nice Film With A Very Long Name");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_container (Ratio::from_id ("239"));
	film->_isdcf_date = boost::gregorian::date (2014, boost::gregorian::Jul, 4);
	film->set_audio_channels (1);
	film->set_resolution (RESOLUTION_4K);
	m.content_version = 2;
	m.audio_language = "DE";
	m.subtitle_language = "FR";
	m.territory = "US";
	m.rating = "R";
	m.studio = "DI";
	m.facility = "PP";
	film->set_isdcf_metadata (m);
	film->set_interop (false);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_TLR-2_S_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* Test interior aspect ratio: shouldn't be shown with trailers */

	shared_ptr<ImageContent> content (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	film->examine_and_add_content (content);
	wait_for_jobs ();
	content->video->set_scale (VideoContentScale (Ratio::from_id ("133")));
	film->set_container (Ratio::from_id ("185"));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_TLR-2_F_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* But should be shown for anything else */

	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("XSN"));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-133_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* And it should always be numeric */

	content->video->set_scale (VideoContentScale (Ratio::from_id ("239")));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-239_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");
	content->video->set_scale (VideoContentScale (Ratio::from_id ("133")));

	/* Test 3D */

	film->set_three_d (true);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2-3D_F-133_DE-fr_US-R_4K_DI_20140704_PP_SMPTE-3D_OV");

	/* Test content type modifiers */

	film->set_three_d (false);
	m.temp_version = true;
	m.pre_release = true;
	m.red_band = true;
	m.chain = "MyChain";
	m.two_d_version_of_three_d = true;
	m.mastered_luminance = "4fl";
	film->set_isdcf_metadata (m);
	film->set_video_frame_rate (48);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2-Temp-Pre-RedBand-MyChain-2D-4fl-48_F-133_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* Test a name which is already in camelCase */

	film->set_three_d (false);
	m.temp_version = false;
	m.pre_release = false;
	m.red_band = false;
	m.chain = "";
	m.two_d_version_of_three_d = false;
	m.mastered_luminance = "";
	film->set_isdcf_metadata (m);
	film->set_video_frame_rate (24);
	film->set_name ("IKnowCamels");
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "IKnowCamels_XSN-2_F-133_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* And one in capitals */

	film->set_name ("LIKE SHOUTING");
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_4K_DI_20140704_PP_SMPTE_OV");

	/* Test audio channel markup */

	film->set_audio_channels (6);
	shared_ptr<FFmpegContent> sound (new FFmpegContent (film, "test/data/sine_440.wav"));
	film->examine_and_add_content (sound);
	wait_for_jobs ();
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_10_4K_DI_20140704_PP_SMPTE_OV");

	AudioMapping mapping = sound->audio->mapping ();

	mapping.set (0, dcp::LEFT, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_20_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::RIGHT, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_30_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::LFE, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_31_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::LS, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_41_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::RS, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51_4K_DI_20140704_PP_SMPTE_OV");
	film->set_audio_channels (8);
	mapping.set (0, dcp::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_61_4K_DI_20140704_PP_SMPTE_OV");
	mapping.set (0, dcp::VI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_71_4K_DI_20140704_PP_SMPTE_OV");
}
