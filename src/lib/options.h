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
#include "util.h"

/** @class Options
 *  @brief Options for a transcoding operation.
 *
 *  These are settings which may be different, in different circumstances, for
 *  the same film; ie they are options for a particular transcode operation.
 */
class Options
{
public:

	Options (std::string f, std::string e, std::string m)
		: padding (0)
		, apply_crop (true)
		, decode_video_frequency (0)
		, decode_audio (true)
		, _frame_out_path (f)
		, _frame_out_extension (e)
		, _multichannel_audio_out_path (m)
	{}

	/** @return The path to write video frames to */
	std::string frame_out_path () const {
		return _frame_out_path;
	}

	/** @param f Frame index.
	 *  @param t true to return a temporary file path, otherwise a permanent one.
	 *  @return The path to write this video frame to.
	 */
	std::string frame_out_path (int f, bool t) const {
		std::stringstream s;
		s << _frame_out_path << "/";
		s.width (8);
		s << std::setfill('0') << f << _frame_out_extension;

		if (t) {
			s << ".tmp";
		}

		return s.str ();
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
	float ratio;                ///< ratio of the wanted output image (not considering padding)
	int padding;                ///< number of pixels of padding (in terms of the output size) each side of the image
	bool apply_crop;            ///< true to apply cropping
	int black_after;            ///< first frame for which to output a black frame, rather than the actual video content, or 0 for none
	int decode_video_frequency; ///< skip frames so that this many are decoded in all (or 0) (for generating thumbnails)
	bool decode_audio;          ///< true to decode audio, otherwise false

private:
	/** Path of the directory to write video frames to */
	std::string _frame_out_path;
	/** Extension to use for video frame files (including the leading .) */
	std::string _frame_out_extension;
	/** Path of the directory to write audio files to */
	std::string _multichannel_audio_out_path;
};
