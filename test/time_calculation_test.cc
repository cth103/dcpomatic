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

#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/player.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using boost::shared_ptr;

static string const xml = "<Content>"
	"<Type>FFmpeg</Type>"
	"<BurnSubtitles>0</BurnSubtitles>"
	"<BitsPerPixel>8</BitsPerPixel>"
	"<Path>test/data/red_24.mp4</Path>"
	"<Digest>2760e03c7251480f7f02c01a907792673784335</Digest>"
	"<Position>0</Position>"
	"<TrimStart>0</TrimStart>"
	"<TrimEnd>0</TrimEnd>"
	"<VideoLength>1353600</VideoLength>"
	"<VideoWidth>1280</VideoWidth>"
	"<VideoHeight>720</VideoHeight>"
	"<VideoFrameRate>25</VideoFrameRate>"
	"<VideoFrameType>0</VideoFrameType>"
	"<LeftCrop>0</LeftCrop>"
	"<RightCrop>0</RightCrop>"
	"<TopCrop>0</TopCrop>"
	"<BottomCrop>0</BottomCrop>"
	"<Scale>"
	"<Ratio>178</Ratio>"
	"</Scale>"
	"<ColourConversion>"
	"<InputTransferFunction>"
	"<Type>ModifiedGamma</Type>"
	"<Power>2.222222222222222</Power>"
	"<Threshold>0.081</Threshold>"
	"<A>0.099</A>"
	"<B>4.5</B>"
	"</InputTransferFunction>"
	"<RedX>0.64</RedX>"
	"<RedY>0.33</RedY>"
	"<GreenX>0.3</GreenX>"
	"<GreenY>0.6</GreenY>"
	"<BlueX>0.15</BlueX>"
	"<BlueY>0.06</BlueY>"
	"<WhiteX>0.3127</WhiteX>"
	"<WhiteY>0.329</WhiteY>"
	"<OutputGamma>2.6</OutputGamma>"
	"</ColourConversion>"
	"<FadeIn>0</FadeIn>"
	"<FadeOut>0</FadeOut>"
	"<AudioGain>0</AudioGain>"
	"<AudioDelay>0</AudioDelay>"
	"<UseSubtitles>0</UseSubtitles>"
	"<SubtitleXOffset>0</SubtitleXOffset>"
	"<SubtitleYOffset>0</SubtitleYOffset>"
	"<SubtitleXScale>1</SubtitleXScale>"
	"<SubtitleYScale>1</SubtitleYScale>"
	"<SubtitleLanguage></SubtitleLanguage>"
	"<AudioStream>"
	"<Selected>1</Selected>"
	"<Name>und; 2 channels</Name>"
	"<Id>2</Id>"
	"<FrameRate>44100</FrameRate>"
	"<Channels>2</Channels>"
	"<FirstAudio>0</FirstAudio>"
	"<Mapping>"
	"<InputChannels>2</InputChannels>"
	"<OutputChannels>12</OutputChannels>"
	"<Gain Input=\"0\" Output=\"0\">1</Gain>"
	"<Gain Input=\"0\" Output=\"1\">0</Gain>"
	"<Gain Input=\"0\" Output=\"2\">0</Gain>"
	"<Gain Input=\"0\" Output=\"3\">0</Gain>"
	"<Gain Input=\"0\" Output=\"4\">0</Gain>"
	"<Gain Input=\"0\" Output=\"5\">0</Gain>"
	"<Gain Input=\"0\" Output=\"6\">0</Gain>"
	"<Gain Input=\"0\" Output=\"7\">0</Gain>"
	"<Gain Input=\"0\" Output=\"8\">0</Gain>"
	"<Gain Input=\"0\" Output=\"9\">0</Gain>"
	"<Gain Input=\"0\" Output=\"10\">0</Gain>"
	"<Gain Input=\"0\" Output=\"11\">0</Gain>"
	"<Gain Input=\"1\" Output=\"0\">0</Gain>"
	"<Gain Input=\"1\" Output=\"1\">1</Gain>"
	"<Gain Input=\"1\" Output=\"2\">0</Gain>"
	"<Gain Input=\"1\" Output=\"3\">0</Gain>"
	"<Gain Input=\"1\" Output=\"4\">0</Gain>"
	"<Gain Input=\"1\" Output=\"5\">0</Gain>"
	"<Gain Input=\"1\" Output=\"6\">0</Gain>"
	"<Gain Input=\"1\" Output=\"7\">0</Gain>"
	"<Gain Input=\"1\" Output=\"8\">0</Gain>"
	"<Gain Input=\"1\" Output=\"9\">0</Gain>"
	"<Gain Input=\"1\" Output=\"10\">0</Gain>"
	"<Gain Input=\"1\" Output=\"11\">0</Gain>"
	"</Mapping>"
	"</AudioStream>"
	"<FirstVideo>0</FirstVideo>"
	"</Content>";

BOOST_AUTO_TEST_CASE (ffmpeg_time_calculation_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_time_calculation_test");

	shared_ptr<cxml::Document> doc (new cxml::Document);
	doc->read_string (xml);

	list<string> notes;
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, doc, film->state_version(), notes));

	/* 25fps content, 25fps DCP */
	film->set_video_frame_rate (25);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (content->video_length() / 25.0));
	/* 25fps content, 24fps DCP; length should be increased */
	film->set_video_frame_rate (24);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (content->video_length() / 24.0));
	/* 25fps content, 30fps DCP; length should be decreased */
	film->set_video_frame_rate (30);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (content->video_length() / 30.0));
	/* 25fps content, 50fps DCP; length should be the same */
	film->set_video_frame_rate (50);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (content->video_length() / 25.0));
	/* 25fps content, 60fps DCP; length should be decreased */
	film->set_video_frame_rate (60);
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (content->video_length() * (50.0 / 60) / 25.0));
}

/** Test Player::dcp_to_content_video */
BOOST_AUTO_TEST_CASE (player_time_calculation_test1)
{
	shared_ptr<Film> film = new_test_film ("player_time_calculation_test1");

	shared_ptr<cxml::Document> doc (new cxml::Document);
	doc->read_string (xml);

	list<string> notes;
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, doc, film->state_version(), notes));
	film->set_sequence_video (false);
	film->add_content (content);

	shared_ptr<Player> player (new Player (film, film->playlist ()));

	/* Position 0, no trim, content rate = DCP rate */
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	shared_ptr<Piece> piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.5)), 12);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.0)), 72);

	/* Position 3s, no trim, content rate = DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)),  36);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 162);

	/* Position 3s, 1.5s trim, content rate = DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),  36);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)),  72);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 198);

	/* Position 0, no trim, content rate 24, DCP rate 25.
	   Now, for example, a DCPTime position of 3s means 3s at 25fps.  Since we run the video
	   fast (at 25fps) in this case, this means 75 frames of content video will be used.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.6)), 15);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.0)), 75);

	/* Position 3s, no trim, content rate 24, DCP rate 25 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.60)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.60)),  40);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 168);

	/* Position 3s, 1.6s trim, content rate 24, DCP rate 25.  Here the trim is in ContentTime,
	   so it's 1.6s at 24fps.
	 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.6));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.60)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),  38);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.60)),  78);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 207);

	/* Position 0, no trim, content rate 24, DCP rate 48
	   Now, for example, a DCPTime position of 3s means 3s at 48fps.  Since we run the video
	   with repeated frames in this case, 3 * 24 frames of content video will
	   be used to make 3 * 48 frames of DCP video.  The results should be the same as the
	   content rate = DCP rate case.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.5)), 12);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.0)), 72);

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)),  36);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 162);

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),  36);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)),  72);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 198);

	/* Position 0, no trim, content rate 48, DCP rate 24
	   Now, for example, a DCPTime position of 3s means 3s at 24fps.  Since we run the video
	   with skipped frames in this case, 3 * 48 frames of content video will
	   be used to make 3 * 24 frames of DCP video.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.5)), 24);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.0)), 144);

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)),  72);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 324);

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (3.00)),  72);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (4.50)), 144);
	BOOST_CHECK_EQUAL (player->dcp_to_content_video (piece, DCPTime::from_seconds (9.75)), 396);
}

/** Test Player::content_video_to_dcp */
BOOST_AUTO_TEST_CASE (player_time_calculation_test2)
{
	shared_ptr<Film> film = new_test_film ("player_time_calculation_test2");

	shared_ptr<cxml::Document> doc (new cxml::Document);
	doc->read_string (xml);

	list<string> notes;
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, doc, film->state_version(), notes));
	film->set_sequence_video (false);
	film->add_content (content);

	shared_ptr<Player> player (new Player (film, film->playlist ()));

	/* Position 0, no trim, content rate = DCP rate */
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	shared_ptr<Piece> piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime ());
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 12), DCPTime::from_seconds (0.5));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (3.0));

	/* Position 3s, no trim, content rate = DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 36), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 162), DCPTime::from_seconds (9.75));

	/* Position 3s, 1.5s trim, content rate = DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (1.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 36), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 198), DCPTime::from_seconds (9.75));

	/* Position 0, no trim, content rate 24, DCP rate 25.
	   Now, for example, a DCPTime position of 3s means 3s at 25fps.  Since we run the video
	   fast (at 25fps) in this case, this means 75 frames of content video will be used.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime ());
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 15), DCPTime::from_seconds (0.6));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 75), DCPTime::from_seconds (3.0));

	/* Position 3s, no trim, content rate 24, DCP rate 25 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 40), DCPTime::from_seconds (4.60));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 169), DCPTime::from_seconds (9.76));

	/* Position 3s, 1.6s trim, content rate 24, DCP rate 25, so the 1.6s trim is at 24fps */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.6));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (1.464));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 40), DCPTime::from_seconds (3.064));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 80), DCPTime::from_seconds (4.664));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 209), DCPTime::from_seconds (9.824));

	/* Position 0, no trim, content rate 24, DCP rate 48
	   Now, for example, a DCPTime position of 3s means 3s at 48fps.  Since we run the video
	   with repeated frames in this case, 3 * 24 frames of content video will
	   be used to make 3 * 48 frames of DCP video.  The results should be the same as the
	   content rate = DCP rate case.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime ());
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 12), DCPTime::from_seconds (0.5));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (3.0));

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 36), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 162), DCPTime::from_seconds (9.75));

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (1.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 36), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 198), DCPTime::from_seconds (9.75));

	/* Position 0, no trim, content rate 48, DCP rate 24
	   Now, for example, a DCPTime position of 3s means 3s at 24fps.  Since we run the video
	   with skipped frames in this case, 3 * 48 frames of content video will
	   be used to make 3 * 24 frames of DCP video.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime ());
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 24), DCPTime::from_seconds (0.5));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 144), DCPTime::from_seconds (3.0));

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 324), DCPTime::from_seconds (9.75));

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (48);
	film->set_video_frame_rate (24);
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 0), DCPTime::from_seconds (1.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 72), DCPTime::from_seconds (3.00));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 144), DCPTime::from_seconds (4.50));
	BOOST_CHECK_EQUAL (player->content_video_to_dcp (piece, 396), DCPTime::from_seconds (9.75));
}

/** Test Player::dcp_to_content_audio */
BOOST_AUTO_TEST_CASE (player_time_calculation_test3)
{
	shared_ptr<Film> film = new_test_film ("player_time_calculation_test3");

	shared_ptr<cxml::Document> doc (new cxml::Document);
	doc->read_string (xml);

	list<string> notes;
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, doc, film->state_version(), notes));
	AudioStreamPtr stream = content->audio_streams().front();
	film->set_sequence_video (false);
	film->add_content (content);

	shared_ptr<Player> player (new Player (film, film->playlist ()));

	/* Position 0, no trim, video/audio content rate = video/audio DCP rate */
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	shared_ptr<Piece> piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.5)),  24000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.0)), 144000);

	/* Position 3s, no trim, video/audio content rate = video/audio DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 324000);

	/* Position 3s, 1.5s trim, video/audio content rate = video/audio DCP rate */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)), 144000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 396000);

	/* Position 0, no trim, content video rate 24, DCP video rate 25, both audio rates still 48k */
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.6)),  28800);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.0)), 144000);

	/* Position 3s, no trim, content video rate 24, DCP rate 25, both audio rates still 48k. */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.60)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.60)),  76800);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 324000);

	/* Position 3s, 1.6s trim, content rate 24, DCP rate 25, both audio rates still 48k.
	   Since the DCP is faster, and resampled audio is at the DCP rate, our 1.6s trim in
	   content time corresponds to 1.6 * 24 * 48000 / 25 audio samples.
	*/
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.6));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (25);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.60)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),  73728);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.60)), 150528);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 397728);

	/* Position 0, no trim, content rate 24, DCP rate 48, both audio rates still 48k.
	   Now, for example, a DCPTime position of 3s means 3s at 48fps.  Since we run the video
	   with repeated frames in this case, audio samples will map straight through.
	   The results should be the same as the content rate = DCP rate case.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.5)),  24000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.0)), 144000);

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 324000);

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)), 144000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 396000);

	/* Position 0, no trim, content rate 48, DCP rate 24
	   Now, for example, a DCPTime position of 3s means 3s at 24fps.  Since we run the video
	   with skipped frames in this case, audio samples should map straight through.
	*/
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (48);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.5)),  24000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.0)), 144000);

	/* Position 3s, no trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 324000);

	/* Position 3s, 1.5s trim, content rate 24, DCP rate 48 */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),   0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)), 144000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 396000);

	/* Position 0, no trim, video content rate = video DCP rate, content audio rate = 44.1k */
	content->set_position (DCPTime ());
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 44100;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.5)),  24000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.0)), 144000);

	/* Position 3s, no trim, video content rate = video DCP rate, content audio rate = 44.1k */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime ());
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 44100;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 324000);

	/* Position 3s, 1.5s trim, video content rate = video DCP rate, content audio rate = 44.1k */
	content->set_position (DCPTime::from_seconds (3));
	content->set_trim_start (ContentTime::from_seconds (1.5));
	content->set_video_frame_rate (24);
	film->set_video_frame_rate (24);
	stream->_frame_rate = 44100;
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_pieces.size(), 1);
	piece = player->_pieces.front ();
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime ()), 0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (0.50)),      0);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (3.00)),  72000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (4.50)), 144000);
	BOOST_CHECK_EQUAL (player->dcp_to_resampled_audio (piece, DCPTime::from_seconds (9.75)), 396000);
}
