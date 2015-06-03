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
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_time_calculation_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_time_calculation_test");

	string const xml = "<Content>"
		"<Type>FFmpeg</Type>"
		"<Path>/home/c.hetherington/DCP/clapperboard.mp4</Path>"
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
		"<Gain Content=\"0\" DCP=\"0\">1</Gain>"
		"<Gain Content=\"0\" DCP=\"1\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"2\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"3\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"4\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"5\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"6\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"7\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"8\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"9\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"10\">0</Gain>"
		"<Gain Content=\"0\" DCP=\"11\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"0\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"1\">1</Gain>"
		"<Gain Content=\"1\" DCP=\"2\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"3\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"4\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"5\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"6\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"7\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"8\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"9\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"10\">0</Gain>"
		"<Gain Content=\"1\" DCP=\"11\">0</Gain>"
		"</Mapping>"
		"</AudioStream>"
		"<FirstVideo>0</FirstVideo>"
		"</Content>";

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

/* XXX much more stuff to test */
