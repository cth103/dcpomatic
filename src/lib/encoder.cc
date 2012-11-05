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

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include "encoder.h"
#include "util.h"
#include "options.h"

using std::pair;
using namespace boost;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding.
 *  @param o Options.
 */
Encoder::Encoder (shared_ptr<const Film> f, shared_ptr<const Options> o)
	: _film (f)
	, _opt (o)
	, _just_skipped (false)
	, _last_frame (0)
{

}


/** @return an estimate of the current number of frames we are encoding per second,
 *  or 0 if not known.
 */
float
Encoder::current_frames_per_second () const
{
	boost::mutex::scoped_lock lock (_history_mutex);
	if (int (_time_history.size()) < _history_size) {
		return 0;
	}

	struct timeval now;
	gettimeofday (&now, 0);

	return _history_size / (seconds (now) - seconds (_time_history.back ()));
}

/** @return true if the last frame to be processed was skipped as it already existed */
bool
Encoder::skipping () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _just_skipped;
}

/** @return Index of last frame to be successfully encoded */
SourceFrame
Encoder::last_frame () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _last_frame;
}

/** Should be called when a frame has been encoded successfully.
 *  @param n Source frame index.
 */
void
Encoder::frame_done (SourceFrame n)
{
	boost::mutex::scoped_lock lock (_history_mutex);
	_just_skipped = false;
	_last_frame = n;
	
	struct timeval tv;
	gettimeofday (&tv, 0);
	_time_history.push_front (tv);
	if (int (_time_history.size()) > _history_size) {
		_time_history.pop_back ();
	}
}

/** Called by a subclass when it has just skipped the processing
    of a frame because it has already been done.
*/
void
Encoder::frame_skipped ()
{
	boost::mutex::scoped_lock lock (_history_mutex);
	_just_skipped = true;
}

void
Encoder::process_video (shared_ptr<const Image> i, SourceFrame f, boost::shared_ptr<Subtitle> s)
{
	if (_opt->decode_video_skip != 0 && (f % _opt->decode_video_skip) != 0) {
		return;
	}

	if (_opt->video_decode_range) {
		pair<SourceFrame, SourceFrame> const r = _opt->video_decode_range.get();
		if (f < r.first || f >= r.second) {
			return;
		}
	}

	do_process_video (i, f, s);
}

void
Encoder::process_audio (shared_ptr<const AudioBuffers> data, int64_t f)
{
	if (_opt->audio_decode_range) {

		shared_ptr<AudioBuffers> trimmed (new AudioBuffers (*data.get ()));
		
		/* Range that we are encoding */
		pair<int64_t, int64_t> required_range = _opt->audio_decode_range.get();
		/* Range of this block of data */
		pair<int64_t, int64_t> this_range (f, f + trimmed->frames());

		if (this_range.second < required_range.first || required_range.second < this_range.first) {
			/* No part of this audio is within the required range */
			return;
		} else if (required_range.first >= this_range.first && required_range.first < this_range.second) {
			/* Trim start */
			int64_t const shift = required_range.first - this_range.first;
			trimmed->move (shift, 0, trimmed->frames() - shift);
			trimmed->set_frames (trimmed->frames() - shift);
		} else if (required_range.second >= this_range.first && required_range.second < this_range.second) {
			/* Trim end */
			trimmed->set_frames (required_range.second - this_range.first);
		}

		data = trimmed;
	}

	do_process_audio (data);
}
