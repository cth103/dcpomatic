/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
	explicit AudioMerger (int frame_rate);

	std::list<std::pair<std::shared_ptr<AudioBuffers>, dcpomatic::DCPTime>> pull (dcpomatic::DCPTime time);
	void push (std::shared_ptr<const AudioBuffers> audio, dcpomatic::DCPTime time);
	void clear ();

private:
	Frame frames (dcpomatic::DCPTime t) const;

	class Buffer
	{
	public:
		/** @param c Channels
		 *  @param f Frames
		 *  @param t Time
		 *  @param r Frame rate.
		 */
		Buffer (int c, int32_t f, dcpomatic::DCPTime t, int r)
			: audio (std::make_shared<AudioBuffers>(c, f))
			, time (t)
			, frame_rate (r)
		{}

		Buffer (std::shared_ptr<AudioBuffers> a, dcpomatic::DCPTime t, int r)
			: audio (a)
			, time (t)
			, frame_rate (r)
		{}

		std::shared_ptr<AudioBuffers> audio;
		dcpomatic::DCPTime time;
		int frame_rate;

		dcpomatic::DCPTimePeriod period () const {
			return dcpomatic::DCPTimePeriod (time, time + dcpomatic::DCPTime::from_frames (audio->frames(), frame_rate));
		}
	};

	std::list<Buffer> _buffers;
	int _frame_rate;
};
