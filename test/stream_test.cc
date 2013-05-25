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

BOOST_AUTO_TEST_CASE (stream_test)
{
#if 0	
	FFmpegAudioStream a ("ffmpeg 4 44100 1 hello there world", boost::optional<int> (1));
	BOOST_CHECK_EQUAL (a.id(), 4);
	BOOST_CHECK_EQUAL (a.sample_rate(), 44100);
	BOOST_CHECK_EQUAL (a.channel_layout(), 1);
	BOOST_CHECK_EQUAL (a.name(), "hello there world");
	BOOST_CHECK_EQUAL (a.to_string(), "ffmpeg 4 44100 1 hello there world");

	SndfileStream e ("external 44100 1", boost::optional<int> (1));
	BOOST_CHECK_EQUAL (e.sample_rate(), 44100);
	BOOST_CHECK_EQUAL (e.channel_layout(), 1);
	BOOST_CHECK_EQUAL (e.to_string(), "external 44100 1");

	SubtitleStream s ("5 a b c", boost::optional<int> (1));
	BOOST_CHECK_EQUAL (s.id(), 5);
	BOOST_CHECK_EQUAL (s.name(), "a b c");

	shared_ptr<AudioStream> ff = audio_stream_factory ("ffmpeg 4 44100 1 hello there world", boost::optional<int> (1));
	shared_ptr<FFmpegAudioStream> cff = dynamic_pointer_cast<FFmpegAudioStream> (ff);
	BOOST_CHECK (cff);
	BOOST_CHECK_EQUAL (cff->id(), 4);
	BOOST_CHECK_EQUAL (cff->sample_rate(), 44100);
	BOOST_CHECK_EQUAL (cff->channel_layout(), 1);
	BOOST_CHECK_EQUAL (cff->name(), "hello there world");
	BOOST_CHECK_EQUAL (cff->to_string(), "ffmpeg 4 44100 1 hello there world");

	shared_ptr<AudioStream> fe = audio_stream_factory ("external 44100 1", boost::optional<int> (1));
	BOOST_CHECK_EQUAL (fe->sample_rate(), 44100);
	BOOST_CHECK_EQUAL (fe->channel_layout(), 1);
	BOOST_CHECK_EQUAL (fe->to_string(), "external 44100 1");
#endif	
}

