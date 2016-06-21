/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "audio_delay.h"
#include "audio_buffers.h"
#include "dcpomatic_assert.h"
#include <iostream>

using std::cout;
using boost::shared_ptr;

AudioDelay::AudioDelay (int samples)
	: _samples (samples)
{

}

shared_ptr<AudioBuffers>
AudioDelay::run (shared_ptr<const AudioBuffers> in)
{
	/* You can't call this with varying channel counts */
	DCPOMATIC_ASSERT (!_tail || in->channels() == _tail->channels());

	shared_ptr<AudioBuffers> out (new AudioBuffers (in->channels(), in->frames()));

	if (in->frames() > _samples) {

		if (!_tail) {
			/* No tail; use silence */
			out->make_silent (0, _samples);
		} else {
			/* Copy tail */
			out->copy_from (_tail.get(), _samples, 0, 0);
		}

		/* Copy in to out */
		out->copy_from (in.get(), in->frames() - _samples, 0, _samples);

		/* Keep tail */
		if (!_tail) {
			_tail.reset (new AudioBuffers (in->channels(), _samples));
		}
		_tail->copy_from (in.get(), _samples, in->frames() - _samples, 0);

	} else {

		/* First part of the tail into the output */
		if (_tail) {
			out->copy_from (_tail.get(), out->frames(), 0, 0);
		} else {
			out->make_silent ();
			_tail.reset (new AudioBuffers (out->channels(), _samples));
			_tail->make_silent ();
		}

		/* Shuffle the tail down */
		_tail->move (out->frames(), 0, _tail->frames() - out->frames());

		/* Copy input into the tail */
		_tail->copy_from (in.get(), in->frames(), 0, _tail->frames() - in->frames());
	}

	return out;
}

void
AudioDelay::flush ()
{
	_tail.reset ();
}
