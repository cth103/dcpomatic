/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_ring_buffers.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include <iostream>


using std::min;
using std::cout;
using std::make_pair;
using std::pair;
using std::list;
using std::shared_ptr;
using boost::optional;
using namespace dcpomatic;


AudioRingBuffers::AudioRingBuffers()
{

}


/** @param frame_rate Frame rate in use; this is only used to check timing consistency of the incoming data */
void
AudioRingBuffers::put(shared_ptr<const AudioBuffers> data, DCPTime time, int frame_rate)
{
	boost::mutex::scoped_lock lm(_mutex);

	if (!_buffers.empty()) {
		DCPOMATIC_ASSERT(_buffers.front().first->channels() == data->channels());
		DCPTime const end = (_buffers.back().second + DCPTime::from_frames(_buffers.back().first->frames(), frame_rate));
		if (labs(end.get() - time.get()) > 1) {
			cout << "bad put " << to_string(_buffers.back().second) << " " << _buffers.back().first->frames() << " " << to_string(time) << "\n";
		}
		DCPOMATIC_ASSERT(labs(end.get() - time.get()) < 2);
	}

	_buffers.push_back(make_pair(data, time));
}


/** @return time of the returned data; if it's not set this indicates an underrun */
optional<DCPTime>
AudioRingBuffers::get(float* out, int channels, int frames)
{
	boost::mutex::scoped_lock lm(_mutex);

	optional<DCPTime> time;

	while (frames > 0) {
		if (_buffers.empty()) {
			for (int i = 0; i < frames; ++i) {
				for (int j = 0; j < channels; ++j) {
					*out++ = 0;
				}
			}
			return time;
		}

		auto front = _buffers.front();
		if (!time) {
			time = front.second + DCPTime::from_frames(_used_in_head, 48000);
		}

		int const to_do = min(frames, front.first->frames() - _used_in_head);
		float* const* p = front.first->data();
		int const c = min(front.first->channels(), channels);
		for (int i = 0; i < to_do; ++i) {
			for (int j = 0; j < c; ++j) {
				*out++ = p[j][i + _used_in_head];
			}
			for (int j = c; j < channels; ++j) {
				*out++ = 0;
			}
		}
		_used_in_head += to_do;
		frames -= to_do;

		if (_used_in_head == front.first->frames()) {
			_buffers.pop_front();
			_used_in_head = 0;
		}
	}

	return time;
}


optional<DCPTime>
AudioRingBuffers::peek() const
{
	boost::mutex::scoped_lock lm(_mutex);
	if (_buffers.empty()) {
		return {};
	}
	return _buffers.front().second;
}


void
AudioRingBuffers::clear()
{
	boost::mutex::scoped_lock lm(_mutex);
	_buffers.clear();
	_used_in_head = 0;
}


Frame
AudioRingBuffers::size() const
{
	boost::mutex::scoped_lock lm(_mutex);
	Frame s = 0;
	for (auto const& i: _buffers) {
		s += i.first->frames();
	}
	return s - _used_in_head;
}
