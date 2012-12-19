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

/** @file src/options.h
 *  @brief Options for a transcoding operation.
 */

#include <string>
#include <iomanip>
#include <sstream>
#include <boost/optional.hpp>
#include "util.h"

/** @class EncodeOptions
 *  @brief EncodeOptions for an encoding operation.
 *
 *  These are settings which may be different, in different circumstances, for
 *  the same film; ie they are options for a particular operation.
 */
class EncodeOptions
{
public:

	EncodeOptions (std::string f, std::string e, std::string m)
		: padding (0)
		, video_skip (0)
		, _frame_out_path (f)
		, _frame_out_extension (e)
		, _multichannel_audio_out_path (m)
	{}

	/** @return The path to write video frames to */
	std::string frame_out_path () const {
		return _frame_out_path;
	}

	/** @param f Source frame index.
	 *  @param t true to return a temporary file path, otherwise a permanent one.
	 *  @return The path to write this video frame to.
	 */
	std::string frame_out_path (SourceFrame f, bool t) const {
		std::stringstream s;
		s << _frame_out_path << "/";
		s.width (8);
		s << std::setfill('0') << f << _frame_out_extension;

		if (t) {
			s << ".tmp";
		}

		return s.str ();
	}

	std::string hash_out_path (SourceFrame f, bool t) const {
		return frame_out_path (f, t) + ".md5";
	}

	/** @return Path to write multichannel audio data to */
	std::string multichannel_audio_out_path () const {
		return _multichannel_audio_out_path;
	}

	/** @param c Audio channel index.
	 *  @param t true to return a temporary file path, otherwise a permanent one.
	 *  @return The path to write this audio file to.
	 */
	std::string multichannel_audio_out_path (int c, bool t) const {
		std::stringstream s;
		s << _multichannel_audio_out_path << "/" << (c + 1) << ".wav";
		if (t) {
			s << ".tmp";
		}

		return s.str ();
	}

	Size out_size;              ///< size of output images
	int padding;                ///< number of pixels of padding (in terms of the output size) each side of the image

	/** Range of video frames to encode (in DCP frames) */
	boost::optional<std::pair<int, int> > video_range;
	/** Range of audio frames to decode (in the DCP's sampling rate) */
	boost::optional<std::pair<int64_t, int64_t> > audio_range;
	
	/** Skip frames such that we don't decode any frame where (index % decode_video_skip) != 0; e.g.
	 *  1 for every frame, 2 for every other frame, etc.
	 */
	SourceFrame video_skip; 

private:
	/** Path of the directory to write video frames to */
	std::string _frame_out_path;
	/** Extension to use for video frame files (including the leading .) */
	std::string _frame_out_extension;
	/** Path of the directory to write audio files to */
	std::string _multichannel_audio_out_path;
};


class DecodeOptions
{
public:
	DecodeOptions ()
		: decode_audio (true)
		, decode_subtitles (false)
		, video_sync (true)
	{}
	
	bool decode_audio;
	bool decode_subtitles;
	bool video_sync;
};
