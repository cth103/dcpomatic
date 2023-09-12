/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;


BOOST_AUTO_TEST_CASE (isdcf_name_test)
{
	auto film = new_test_film ("isdcf_name_test");

	/* A basic test */

	film->set_name ("My Nice Film");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->_isdcf_date = boost::gregorian::date (2014, boost::gregorian::Jul, 4);
	auto audio = content_factory("test/data/sine_440.wav")[0];
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	film->set_audio_language(dcp::LanguageTag("en-US"));
	film->set_content_versions({"1"});
	film->set_release_territory(dcp::LanguageTag::RegionSubtag("GB"));
	film->set_ratings({dcp::Rating("BBFC", "PG")});
	film->set_studio (string("ST"));
	film->set_facility (string("FAC"));
	film->set_interop (true);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilm_FTR-1_F_EN-XX_GB-PG_10_2K_ST_20140704_FAC_IOP_OV");

	/* Check that specifying no audio language writes XX */
	film->set_audio_language (boost::none);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilm_FTR-1_F_XX-XX_GB-PG_10_2K_ST_20140704_FAC_IOP_OV");

	/* Test a long name and some different data */

	film->set_name ("My Nice Film With A Very Long Name");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_container (Ratio::from_id ("239"));
	film->_isdcf_date = boost::gregorian::date (2014, boost::gregorian::Jul, 4);
	film->set_audio_channels (1);
	film->set_resolution (Resolution::FOUR_K);
	auto text = content_factory("test/data/subrip.srt")[0];
	BOOST_REQUIRE_EQUAL (text->text.size(), 1U);
	text->text.front()->set_burn (true);
	text->text.front()->set_language (dcp::LanguageTag("fr-FR"));
	film->examine_and_add_content (text);
	film->set_version_number(2);
	film->set_release_territory(dcp::LanguageTag::RegionSubtag("US"));
	film->set_ratings({dcp::Rating("MPA", "R")});
	film->set_studio (string("di"));
	film->set_facility (string("ppfacility"));
	BOOST_REQUIRE (!wait_for_jobs());
	audio = content_factory("test/data/sine_440.wav")[0];
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	film->set_audio_language (dcp::LanguageTag("de-DE"));
	film->set_interop (false);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_TLR-2_S_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* Test to see that RU ratings like 6+ are stripped of their + */
	film->set_ratings({dcp::Rating("RARS", "6+")});
	BOOST_CHECK_EQUAL (film->dcp_name(false), "MyNiceFilmWith_TLR-2_S_DE-fr_US-6_MOS_4K_DI_20140704_PPF_SMPTE_OV");
	film->set_ratings({dcp::Rating("MPA", "R")});

	/* Test interior aspect ratio: shouldn't be shown with trailers */

	shared_ptr<ImageContent> content (new ImageContent ("test/data/simple_testcard_640x480.png"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	content->video->set_custom_ratio (1.33);
	film->set_container (Ratio::from_id ("185"));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_TLR-2_F_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* But should be shown for anything else */

	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("XSN"));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-133_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* And it should always be numeric */

	content->video->set_custom_ratio (2.39);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-239_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	content->video->set_custom_ratio (1.9);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-190_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* And it should be possible to set any 'strange' ratio, not just the ones we know about */
	content->video->set_custom_ratio (2.2);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-220_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");
	content->video->set_custom_ratio (1.95);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2_F-195_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	content->video->set_custom_ratio (1.33);

	/* Test 3D */

	film->set_three_d (true);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2-3D_F-133_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE-3D_OV");

	/* Test content type modifiers */

	film->set_three_d (false);
	film->set_temp_version (true);
	film->set_pre_release (true);
	film->set_red_band (true);
	film->set_two_d_version_of_three_d (true);
	film->set_chain (string("MyChain"));
	film->set_luminance (dcp::Luminance(4.5, dcp::Luminance::Unit::FOOT_LAMBERT));
	film->set_video_frame_rate (48);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "MyNiceFilmWith_XSN-2-Temp-Pre-RedBand-MyChain-2D-4.5fl-48_F-133_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* Test a name which is already in camelCase */

	film->set_three_d (false);
	film->set_temp_version (false);
	film->set_pre_release (false);
	film->set_red_band (false);
	film->set_two_d_version_of_three_d (false);
	film->set_chain (string(""));
	film->set_luminance (boost::none);
	film->set_video_frame_rate (24);
	film->set_name ("IKnowCamels");
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "IKnowCamels_XSN-2_F-133_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* And one in capitals */

	film->set_name ("LIKE SHOUTING");
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_MOS_4K_DI_20140704_PPF_SMPTE_OV");

	/* Test audio channel markup */

	film->set_audio_channels (6);
	auto sound = make_shared<FFmpegContent>("test/data/sine_440.wav");
	film->examine_and_add_content (sound);
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_10_4K_DI_20140704_PPF_SMPTE_OV");

	AudioMapping mapping = sound->audio->mapping ();

	mapping.set (0, dcp::Channel::LEFT, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_20_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::RIGHT, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_30_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::LFE, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_31_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::LS, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_41_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::RS, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51_4K_DI_20140704_PPF_SMPTE_OV");

	film->set_audio_channels (8);
	mapping.set (0, dcp::Channel::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51-HI_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::VI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51-HI-VI_4K_DI_20140704_PPF_SMPTE_OV");

	film->set_audio_channels(10);
	mapping.set (0, dcp::Channel::HI, 0.0);
	mapping.set (0, dcp::Channel::VI, 0.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51-HI_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::VI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_51-HI-VI_4K_DI_20140704_PPF_SMPTE_OV");

	film->set_audio_channels(12);
	mapping.set (0, dcp::Channel::BSL, 1.0);
	mapping.set (0, dcp::Channel::BSR, 1.0);
	mapping.set (0, dcp::Channel::HI, 0.0);
	mapping.set (0, dcp::Channel::VI, 0.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_71_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::HI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_71-HI_4K_DI_20140704_PPF_SMPTE_OV");
	mapping.set (0, dcp::Channel::VI, 1.0);
	sound->audio->set_mapping (mapping);
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_DE-fr_US-R_71-HI-VI_4K_DI_20140704_PPF_SMPTE_OV");

	/* Check that the proper codes are used, not just part of the language code; in this case, QBP instead of PT (#2235) */
	film->set_audio_language(dcp::LanguageTag("pt-BR"));
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_QBP-fr_US-R_71-HI-VI_4K_DI_20140704_PPF_SMPTE_OV");

	/* Check that nothing is added for non-existent ratings */
	film->set_ratings({});
	BOOST_CHECK_EQUAL (film->isdcf_name(false), "LikeShouting_XSN-2_F-133_QBP-fr_US_71-HI-VI_4K_DI_20140704_PPF_SMPTE_OV");
}


BOOST_AUTO_TEST_CASE(isdcf_name_with_atmos)
{
	auto content = content_factory(TestPaths::private_data() / "atmos_asset.mxf");
	auto film = new_test_film2("isdcf_name_with_atmos", content);
	film->_isdcf_date = boost::gregorian::date(2023, boost::gregorian::Jan, 18);
	film->set_name("Hello");

	BOOST_CHECK_EQUAL(film->isdcf_name(false), "Hello_TST-1_F_XX-XX_MOS-IAB_2K_20230118_SMPTE_OV");
}


BOOST_AUTO_TEST_CASE(isdcf_name_with_ccap)
{
	auto content = content_factory("test/data/short.srt")[0];
	auto film = new_test_film2("isdcf_name_with_ccap", { content });
	content->text[0]->set_use(true);
	content->text[0]->set_type(TextType::CLOSED_CAPTION);
	content->text[0]->set_dcp_track(DCPTextTrack("Foo", dcp::LanguageTag("de-DE")));
	film->_isdcf_date = boost::gregorian::date(2023, boost::gregorian::Jan, 18);
	film->set_name("Hello");

	BOOST_CHECK_EQUAL(film->isdcf_name(false), "Hello_TST-1_F_XX-DE-CCAP_MOS_2K_20230118_SMPTE_OV");
}

