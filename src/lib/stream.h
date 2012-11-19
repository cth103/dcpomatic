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

/** @file src/lib/stream.h
 *  @brief Representations of audio and subtitle streams.
 *
 *  Some content may have multiple `streams' of audio and/or subtitles; perhaps
 *  for multiple languages, or for stereo / surround mixes.  These classes represent
 *  those streams, and know about their details.
 */

#ifndef DVDOMATIC_STREAM_H
#define DVDOMATIC_STREAM_H

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
extern "C" {
#include <libavutil/audioconvert.h>
}

/** @class Stream
 *  @brief Parent class for streams.
 */
class Stream
{
public:
	virtual std::string to_string () const = 0;
};

/** @class AudioStream
 *  @brief A stream of audio data.
 */
struct AudioStream : public Stream
{
public:
	AudioStream (int r, int64_t l)
		: _sample_rate (r)
		, _channel_layout (l)
	{}

	/* Only used for backwards compatibility for state file version < 1 */
	void set_sample_rate (int s) {
		_sample_rate = s;
	}

	int channels () const {
		return av_get_channel_layout_nb_channels (_channel_layout);
	}

	int sample_rate () const {
		return _sample_rate;
	}

	int64_t channel_layout () const {
		return _channel_layout;
	}

protected:
	AudioStream ()
		: _sample_rate (0)
		, _channel_layout (0)
	{}

	int _sample_rate;
	int64_t _channel_layout;
};

/** @class SubtitleStream
 *  @brief A stream of subtitle data.
 */
class SubtitleStream : public Stream
{
public:
	SubtitleStream (std::string n, int i)
		: _name (n)
		, _id (i)
	{}

	std::string to_string () const;

	std::string name () const {
		return _name;
	}

	int id () const {
		return _id;
	}

	static boost::shared_ptr<SubtitleStream> create (std::string t, boost::optional<int> v);

private:
	friend class stream_test;
	
	SubtitleStream (std::string t, boost::optional<int> v);
	
	std::string _name;
	int _id;
};

boost::shared_ptr<AudioStream> audio_stream_factory (std::string t, boost::optional<int> version);
boost::shared_ptr<SubtitleStream> subtitle_stream_factory (std::string t, boost::optional<int> version);

#endif
