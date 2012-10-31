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
extern "C" {
#include <libavutil/samplefmt.h>
}

class Options;
class Image;
class Subtitle;
class AudioBuffers;
class Film;

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
	Encoder (boost::shared_ptr<const Film> f, boost::shared_ptr<const Options> o);

	/** Called to indicate that a processing run is about to begin */
	virtual void process_begin (int64_t audio_channel_layout) = 0;

	/** Called with a frame of video.
	 *  @param i Video frame image.
	 *  @param f Frame number within the film.
	 *  @param s A subtitle that should be on this frame, or 0.
	 */
	virtual void process_video (boost::shared_ptr<const Image> i, int f, boost::shared_ptr<Subtitle> s) = 0;

	/** Called with some audio data.
	 *  @param d Array of pointers to floating point sample data for each channel.
	 *  @param s Number of frames (ie number of samples in each channel)
	 */
	virtual void process_audio (boost::shared_ptr<const AudioBuffers>) = 0;

	/** Called when a processing run has finished */
	virtual void process_end () = 0;

	float current_frames_per_second () const;
	bool skipping () const;
	int last_frame () const;

protected:
	void frame_done (int n);
	void frame_skipped ();
	
	/** Film that we are encoding */
	boost::shared_ptr<const Film> _film;
	/** Options */
	boost::shared_ptr<const Options> _opt;

	/** Mutex for _time_history, _just_skipped and _last_frame */
	mutable boost::mutex _history_mutex;
	/** List of the times of completion of the last _history_size frames;
	    first is the most recently completed.
	*/
	std::list<struct timeval> _time_history;
	/** Number of frames that we should keep history for */
	static int const _history_size;
	/** true if the last frame we processed was skipped (because it was already done) */
	bool _just_skipped;
	/** Index of the last frame to be processed */
	int _last_frame;
};

#endif
