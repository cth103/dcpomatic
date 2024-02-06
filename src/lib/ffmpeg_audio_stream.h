/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_stream.h"
#include "dcpomatic_time.h"
#include "ffmpeg_stream.h"


struct ffmpeg_pts_offset_test;


class FFmpegAudioStream : public FFmpegStream, public AudioStream
{
public:
	FFmpegAudioStream(std::string name, int id, int frame_rate, Frame length, int channels, int bit_depth)
		: FFmpegStream (name, id)
		, AudioStream(frame_rate, length, channels, bit_depth)
	{}

	FFmpegAudioStream(std::string name, std::string codec_name_, int id, int frame_rate, Frame length, int channels, int bit_depth)
		: FFmpegStream (name, id)
		, AudioStream(frame_rate, length, channels, bit_depth)
		, codec_name (codec_name_)
	{}

	FFmpegAudioStream(std::string name, int id, int frame_rate, Frame length, AudioMapping mapping, int bit_depth)
		: FFmpegStream (name, id)
		, AudioStream(frame_rate, length, mapping, bit_depth)
	{}

	FFmpegAudioStream (cxml::ConstNodePtr, int);

	void as_xml(xmlpp::Element*) const;

	/* XXX: should probably be locked */

	boost::optional<dcpomatic::ContentTime> first_audio;
	boost::optional<std::string> codec_name;

private:
	friend struct ffmpeg_pts_offset_test;

	/* Constructor for tests */
	FFmpegAudioStream ()
		: FFmpegStream ("", 0)
		, AudioStream(0, 0, 0, 0)
	{}
};

