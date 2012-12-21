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
#define TIMING(...) _film->log()->microsecond_log (String::compose (__VA_ARGS__), Log::TIMING);
#else
#define TIMING(...)
#endif

/** The maximum number of audio channels that we can cope with */
#define MAX_AUDIO_CHANNELS 6

class Scaler;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern void stacktrace (std::ostream &, int);
extern std::string dependency_version_summary ();
extern double seconds (struct timeval);
extern void dvdomatic_setup ();
extern std::vector<std::string> split_at_spaces_considering_quotes (std::string);
extern std::string md5_digest (std::string);
extern std::string md5_digest (void const *, int);
extern void ensure_ui_thread ();

typedef int SourceFrame;

struct DCPFrameRate
{
	/** frames per second for the DCP */
	int frames_per_second;
	/** Skip every `skip' frames.  e.g. if this is 1, we skip nothing;
	 *  if it's 2, we skip every other frame.
	 */
	int skip;
	/** true if this DCP will run its video faster than the source
	 *  (e.g. if the source is 29.97fps and we will run the DCP at 30fps)
	 */
	bool run_fast;
};

enum ContentType {
	STILL, ///< content is still images
	VIDEO  ///< content is a video
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

extern bool operator== (Size const & a, Size const & b);
extern bool operator!= (Size const & a, Size const & b);

/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
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

/** @struct Position
 *  @brief A position.
 */
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

/** @struct Rect
 *  @brief A rectangle.
 */
struct Rect
{
	Rect ()
		: x (0)
		, y (0)
		, width (0)
		, height (0)
	{}

	Rect (int x_, int y_, int w_, int h_)
		: x (x_)
		, y (y_)
		, width (w_)
		, height (h_)
	{}

	int x;
	int y;
	int width;
	int height;

	Position position () const {
		return Position (x, y);
	}

	Size size () const {
		return Size (width, height);
	}

	Rect intersection (Rect const & other) const;
};

extern std::string crop_string (Position, Size);
extern int dcp_audio_sample_rate (int);
extern DCPFrameRate dcp_frame_rate (float);
extern std::string colour_lut_index_to_name (int index);
extern int stride_round_up (int, int const *, int);
extern int stride_lookup (int c, int const * stride);
extern std::multimap<std::string, std::string> read_key_value (std::istream& s);
extern int get_required_int (std::multimap<std::string, std::string> const & kv, std::string k);
extern float get_required_float (std::multimap<std::string, std::string> const & kv, std::string k);
extern std::string get_required_string (std::multimap<std::string, std::string> const & kv, std::string k);
extern int get_optional_int (std::multimap<std::string, std::string> const & kv, std::string k);
extern std::string get_optional_string (std::multimap<std::string, std::string> const & kv, std::string k);

/** @class Socket
 *  @brief A class to wrap a boost::asio::ip::tcp::socket with some things
 *  that are useful for DVD-o-matic.
 *
 *  This class wraps some things that I could not work out how to do with boost;
 *  most notably, sync read/write calls with timeouts, and the ability to peek into
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
	uint8_t _buffer[1024];
	/** amount of valid data in the buffer */
	int _buffer_data;
};

/** @class AudioBuffers
 *  @brief A class to hold multi-channel audio data in float format.
 */
class AudioBuffers
{
public:
	AudioBuffers (int channels, int frames);
	AudioBuffers (AudioBuffers const &);
	~AudioBuffers ();

	float** data () const {
		return _data;
	}
	
	float* data (int) const;

	int channels () const {
		return _channels;
	}

	int frames () const {
		return _frames;
	}

	void set_frames (int f);

	void make_silent ();
	void make_silent (int c);

	void copy_from (AudioBuffers* from, int frames_to_copy, int read_offset, int write_offset);
	void move (int from, int to, int frames);

private:
	/** Number of channels */
	int _channels;
	/** Number of frames (where a frame is one sample across all channels) */
	int _frames;
	/** Number of frames that _data can hold */
	int _allocated_frames;
	/** Audio data (so that, e.g. _data[2][6] is channel 2, sample 6) */
	float** _data;
};

extern int64_t video_frames_to_audio_frames (SourceFrame v, float audio_sample_rate, float frames_per_second);
extern bool still_image_file (std::string);

#endif

