/*
    Copyright (C) 2016-2017 Carl Hetherington <cth@carlh.net>

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
#include <boost/foreach.hpp>

using std::min;
using std::cout;
using boost::shared_ptr;

AudioRingBuffers::AudioRingBuffers ()
	: _used_in_head (0)
{

}

void
AudioRingBuffers::put (shared_ptr<const AudioBuffers> data, DCPTime time)
{
	boost::mutex::scoped_lock lm (_mutex);

	if (!_buffers.empty ()) {
		DCPOMATIC_ASSERT (_buffers.front()->channels() == data->channels());
	}

	_buffers.push_back (data);
	_latest = time;
}

void
AudioRingBuffers::get (float* out, int channels, int frames)
{
	boost::mutex::scoped_lock lm (_mutex);

	while (frames > 0) {
		if (_buffers.empty ()) {
			for (int i = 0; i < frames; ++i) {
				for (int j = 0; j < channels; ++j) {
					*out++ = 0;
				}
			}
			cout << "audio underrun; missing " << frames << "!\n";
			return;
		}

		int const to_do = min (frames, _buffers.front()->frames() - _used_in_head);
		float** p = _buffers.front()->data();
		int const c = min (_buffers.front()->channels(), channels);
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

		if (_used_in_head == _buffers.front()->frames()) {
			_buffers.pop_front ();
			_used_in_head = 0;
		}
	}
}

void
AudioRingBuffers::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_buffers.clear ();
	_used_in_head = 0;
	_latest = DCPTime ();
}

Frame
AudioRingBuffers::size ()
{
	boost::mutex::scoped_lock lm (_mutex);
	Frame s = 0;
	BOOST_FOREACH (shared_ptr<const AudioBuffers> i, _buffers) {
		s += i->frames ();
	}
	return s - _used_in_head;
}
