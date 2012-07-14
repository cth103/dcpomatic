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

#ifndef DVDOMATIC_ENCODER_H
#define DVDOMATIC_ENCODER_H

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <stdint.h>

class FilmState;
class Options;
class Image;
class Log;

/** @class Encoder
 *  @brief Parent class for classes which can encode video and audio frames.
 *
 *  Video is supplied to process_video as YUV frames, and audio
 *  is supplied as uncompressed PCM in blocks of various sizes.
 *
 *  The subclass is expected to encode the video and/or audio in
 *  some way and write it to disk.
 */

class Encoder
{
public:
	Encoder (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Log* l);

	/** Called to indicate that a processing run is about to begin */
	virtual void process_begin () = 0;

	/** Called with a frame of video.
	 *  @param i Video frame image.
	 *  @param f Frame number within the film.
	 */
	virtual void process_video (boost::shared_ptr<Image> i, int f) = 0;

	/** Called with some audio data.
	 *  @param d Data.
	 *  @param s Size of data (in bytes)
	 */
	virtual void process_audio (uint8_t* d, int s) = 0;

	/** Called when a processing run has finished */
	virtual void process_end () = 0;

	float current_frames_per_second () const;

protected:
	void frame_done ();
	
	/** FilmState of the film that we are encoding */
	boost::shared_ptr<const FilmState> _fs;
	/** Options */
	boost::shared_ptr<const Options> _opt;
	/** Log */
	Log* _log;

	mutable boost::mutex _history_mutex;
	std::list<struct timeval> _time_history;
	static int const _history_size;
};

#endif
