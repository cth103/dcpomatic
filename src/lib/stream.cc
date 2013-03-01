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

#include <sstream>
#include "compose.hpp"
#include "stream.h"
#include "ffmpeg_decoder.h"
#include "external_audio_decoder.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using boost::shared_ptr;
using boost::optional;

/** Construct a SubtitleStream from a value returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 */
SubtitleStream::SubtitleStream (string t, boost::optional<int>)
{
	stringstream n (t);
	n >> _id;

	size_t const s = t.find (' ');
	if (s != string::npos) {
		_name = t.substr (s + 1);
	}
}

/** @return A canonical string representation of this stream */
string
SubtitleStream::to_string () const
{
	return String::compose (N_("%1 %2"), _id, _name);
}

/** Create a SubtitleStream from a value returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 */
shared_ptr<SubtitleStream>
SubtitleStream::create (string t, optional<int> v)
{
	return shared_ptr<SubtitleStream> (new SubtitleStream (t, v));
}

/** Create an AudioStream from a string returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 *  @return AudioStream, or 0.
 */
shared_ptr<AudioStream>
audio_stream_factory (string t, optional<int> v)
{
	shared_ptr<AudioStream> s;

	s = FFmpegAudioStream::create (t, v);
	if (!s) {
		s = ExternalAudioStream::create (t, v);
	}

	return s;
}

/** Create a SubtitleStream from a string returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 *  @return SubtitleStream, or 0.
 */
shared_ptr<SubtitleStream>
subtitle_stream_factory (string t, optional<int> v)
{
	return SubtitleStream::create (t, v);
}
