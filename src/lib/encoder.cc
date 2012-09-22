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

using namespace boost;

int const Encoder::_history_size = 25;

/** @param s FilmState of the film that we are encoding.
 *  @param o Options.
 *  @param l Log.
 */
Encoder::Encoder (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: _fs (s)
	, _opt (o)
	, _log (l)
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

bool
Encoder::skipping () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _just_skipped;
}

int
Encoder::last_frame () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _last_frame;
}

void
Encoder::frame_done (int n)
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
