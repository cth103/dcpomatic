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
#include <boost/asio.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#include "compose.hpp"

#ifdef DVDOMATIC_DEBUG
#define TIMING(...) _log->microsecond_log (String::compose (__VA_ARGS__), Log::TIMING);
#else
#define TIMING(...)
#endif

class Scaler;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern void stacktrace (std::ostream &, int);
extern std::string audio_sample_format_to_string (AVSampleFormat);
extern AVSampleFormat audio_sample_format_from_string (std::string);
extern std::string dependency_version_summary ();
extern double seconds (struct timeval);
extern void dvdomatic_setup ();
extern std::vector<std::string> split_at_spaces_considering_quotes (std::string);
extern std::string md5_digest (std::string);
extern std::string md5_digest (void const *, int);

enum ContentType {
	STILL,
	VIDEO
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

/** A description of the crop of an image or video. */
struct Crop
{
	Crop () : left (0), right (0), top (0), bottom (0) {}

	/** Number of pixels to remove from the left-hand side */
	int left;
	/** Number of pixels to remove from the right-hand side */
	int right;
	/** Number of pixels to remove from the top */
	int top;
	/** Number of pixels to remove from the bottom */
	int bottom;
};

extern bool operator== (Crop const & a, Crop const & b);
extern bool operator!= (Crop const & a, Crop const & b);

/** A position */
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

	/** x coordinate */
	int x;
	/** y coordinate */
	int y;
};

extern std::string crop_string (Position, Size);
extern int dcp_audio_sample_rate (int);
extern std::string colour_lut_index_to_name (int index);

/** @class Socket
 *  @brief A class to wrap a boost::asio::ip::tcp::socket with some things
 *  that are useful for DVD-o-matic.
 *
 *  This class wraps some things that I could not work out how to do with boost;
 *  most notably, sync read/write calls with timeouts, and the ability to peak into
 *  data being read.
 */
class Socket
{
public:
	Socket ();

	/** @return Our underlying socket */
	boost::asio::ip::tcp::socket& socket () {
		return _socket;
	}

	void connect (boost::asio::ip::basic_resolver_entry<boost::asio::ip::tcp> const & endpoint, int timeout);
	void write (uint8_t const * data, int size, int timeout);
	
	void read_definite_and_consume (uint8_t* data, int size, int timeout);
	void read_indefinite (uint8_t* data, int size, int timeout);
	void consume (int amount);
	
private:
	void check ();
	int read (uint8_t* data, int size, int timeout);

	Socket (Socket const &);

	boost::asio::io_service _io_service;
	boost::asio::deadline_timer _deadline;
	boost::asio::ip::tcp::socket _socket;
	/** a buffer for small reads */
	uint8_t _buffer[256];
	/** amount of valid data in the buffer */
	int _buffer_data;
};

#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define RGB_TO_Y_CCIR(r, g, b) \
((FIX(0.29900*219.0/255.0) * (r) + FIX(0.58700*219.0/255.0) * (g) + \
  FIX(0.11400*219.0/255.0) * (b) + (ONE_HALF + (16 << SCALEBITS))) >> SCALEBITS)

#define RGB_TO_U_CCIR(r1, g1, b1, shift)\
(((- FIX(0.16874*224.0/255.0) * r1 - FIX(0.33126*224.0/255.0) * g1 +         \
     FIX(0.50000*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V_CCIR(r1, g1, b1, shift)\
(((FIX(0.50000*224.0/255.0) * r1 - FIX(0.41869*224.0/255.0) * g1 -           \
   FIX(0.08131*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#endif
