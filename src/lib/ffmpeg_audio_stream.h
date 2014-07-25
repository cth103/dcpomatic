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
#include "dcpomatic_time.h"

struct ffmpeg_pts_offset_test;

class FFmpegAudioStream : public FFmpegStream
{
public:
	FFmpegAudioStream (std::string n, int i, int f, int c)
		: FFmpegStream (n, i)
		, _frame_rate (f)
		, _channels (c)
		, _mapping (c)
	{
		_mapping.make_default ();
	}

	FFmpegAudioStream (cxml::ConstNodePtr, int);

	void as_xml (xmlpp::Node *) const;

	int frame_rate () const {
		return _frame_rate;
	}
	
	int channels () const {
		return _channels;
	}
	
	AudioMapping mapping () const {
		return _mapping;
	}

	void set_mapping (AudioMapping m) {
		_mapping = m;
	}
	
	boost::optional<ContentTime> first_audio;

private:
	friend struct ffmpeg_pts_offset_test;

	/* Constructor for tests */
	FFmpegAudioStream ()
		: FFmpegStream ("", 0)
		, _frame_rate (0)
		, _channels (0)
		, _mapping (1)
	{}

	int _frame_rate;
	int _channels;
	AudioMapping _mapping;
};
