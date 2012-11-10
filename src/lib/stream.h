/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#ifndef DVDOMATIC_STREAM_H
#define DVDOMATIC_STREAM_H

#include <stdint.h>
extern "C" {
#include <libavutil/audioconvert.h>
}

class Stream
{
public:
	Stream ()
		: _id (-1)
	{}
	
	Stream (std::string n, int i)
		: _name (n)
		, _id (i)
	{}

	virtual std::string to_string () const = 0;
	
	std::string name () const {
		return _name;
	}

	int id () const {
		return _id;
	}

protected:
	std::string _name;
	int _id;
};

struct AudioStream : public Stream
{
public:
	AudioStream (std::string t);
	
	AudioStream (std::string n, int id, int r, int64_t l)
		: Stream (n, id)
		, _sample_rate (r)
		, _channel_layout (l)
	{}

	std::string to_string () const;

	int channels () const {
		return av_get_channel_layout_nb_channels (_channel_layout);
	}

	int sample_rate () const {
		return _sample_rate;
	}

	int64_t channel_layout () const {
		return _channel_layout;
	}

private:
	int _sample_rate;
	int64_t _channel_layout;
};

class SubtitleStream : public Stream
{
public:
	SubtitleStream (std::string t);

	SubtitleStream (std::string n, int i)
		: Stream (n, i)
	{}

	std::string to_string () const;
};

#endif
