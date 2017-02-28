/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  src/audio_merger.h
 *  @brief AudioMerger class.
 */

#include "audio_buffers.h"
#include "dcpomatic_time.h"
#include "util.h"

/** @class AudioMerger.
 *  @brief A class that can merge audio data from many sources.
 */
class AudioMerger
{
public:
	AudioMerger (int frame_rate);

	std::list<std::pair<boost::shared_ptr<AudioBuffers>, DCPTime> > pull (DCPTime time);
	void push (boost::shared_ptr<const AudioBuffers> audio, DCPTime time);

private:
	Frame frames (DCPTime t) const;

	class Buffer
	{
	public:
		/** @param c Channels
		 *  @param f Frames
		 *  @param t Time
		 *  @param r Frame rate.
		 */
		Buffer (int c, int32_t f, DCPTime t, int r)
			: audio (new AudioBuffers (c, f))
			, time (t)
			, frame_rate (r)
		{}

		Buffer (boost::shared_ptr<AudioBuffers> a, DCPTime t, int r)
			: audio (a)
			, time (t)
			, frame_rate (r)
		{}

		boost::shared_ptr<AudioBuffers> audio;
		DCPTime time;
		int frame_rate;

		DCPTimePeriod period () const {
			return DCPTimePeriod (time, time + DCPTime::from_frames (audio->frames(), frame_rate));
		}
	};

	class BufferComparator
	{
	public:
		bool operator() (AudioMerger::Buffer const & a, AudioMerger::Buffer const & b)
		{
			return a.time < b.time;
		}
	};

	std::list<Buffer> _buffers;
	DCPTime _last_pull;
	int _frame_rate;
};
