/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DCPOMATIC_AUDIO_STREAM_H
#define DCPOMATIC_AUDIO_STREAM_H

#include "audio_mapping.h"
#include "types.h"
#include <boost/thread/mutex.hpp>

struct audio_sampling_rate_test;

class AudioStream
{
public:
	AudioStream (int frame_rate, Frame length, int channels);
	AudioStream (int frame_rate, Frame length, AudioMapping mapping);
	virtual ~AudioStream () {}

	void set_mapping (AudioMapping mapping);

	AudioMapping mapping () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _mapping;
	}

	int frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _frame_rate;
	}

	Frame length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _length;
	}

	int channels () const;

protected:
	mutable boost::mutex _mutex;

private:
	friend struct audio_sampling_rate_test;
	friend struct player_time_calculation_test3;

	int _frame_rate;
	Frame _length;
	AudioMapping _mapping;
};

typedef boost::shared_ptr<AudioStream> AudioStreamPtr;

#endif
