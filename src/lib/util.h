/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>
    Copyright (C) 2000-2007 Paul Davis

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

/** @file src/util.h
 *  @brief Some utility functions and classes.
 */

#ifndef DVDOMATIC_UTIL_H
#define DVDOMATIC_UTIL_H

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}

class Scaler;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern void stacktrace (std::ostream &, int);
extern std::string audio_sample_format_to_string (AVSampleFormat);
extern AVSampleFormat audio_sample_format_from_string (std::string);
extern std::string dependency_version_summary ();
extern void socket_write (int, uint8_t const *, int);
extern double seconds (struct timeval);
extern void dvdomatic_setup ();
extern std::vector<std::string> split_at_spaces_considering_quotes (std::string);
extern std::string md5_digest (std::string);

enum ContentType {
	STILL,
	VIDEO
};

#ifdef DEBUG_HASH
extern void md5_data (std::string, void const *, int);
#endif

/** @class SocketReader
 *  @brief A helper class from reading from sockets.
 */
class SocketReader
{
public:
	SocketReader (int);

	void read_definite_and_consume (uint8_t *, int);
	void read_indefinite (uint8_t *, int);
	void consume (int);

private:
	/** file descriptor we are reading from */
	int _fd;
	/** a buffer for small reads */
	uint8_t _buffer[256];
	/** amount of valid data in the buffer */
	int _buffer_data;
};

/** @class Size
 *  @brief Representation of the size of something */
struct Size
{
	/** Construct a zero Size */
	Size ()
		: width (0)
		, height (0)
	{}

	/** @param w Width.
	 *  @param h Height.
	 */
	Size (int w, int h)
		: width (w)
		, height (h)
	{}

	/** width */
	int width;
	/** height */
	int height;
};

struct Position
{
	Position ()
		: x (0)
		, y (0)
	{}

	Position (int x_, int y_)
		: x (x_)
		, y (y_)
	{}

	int x;
	int y;
};

extern std::string crop_string (Position, Size);

#endif
