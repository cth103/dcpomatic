/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_stream.h"
#include "audio_mapping.h"
#include "audio_stream.h"
#include "dcpomatic_time.h"

struct ffmpeg_pts_offset_test;

class FFmpegAudioStream : public FFmpegStream, public AudioStream
{
public:
	FFmpegAudioStream (std::string name, int id, int frame_rate, int channels)
		: FFmpegStream (name, id)
		, AudioStream (frame_rate, channels)
	{}

	FFmpegAudioStream (cxml::ConstNodePtr, int);

	void as_xml (xmlpp::Node *) const;

	/* XXX: should probably be locked */

	boost::optional<ContentTime> first_audio;

private:
	friend struct ffmpeg_pts_offset_test;

	/* Constructor for tests */
	FFmpegAudioStream ()
		: FFmpegStream ("", 0)
		, AudioStream (0, 0)
	{}
};
