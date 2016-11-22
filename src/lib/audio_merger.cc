/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "audio_merger.h"
#include "dcpomatic_time.h"

using std::pair;
using std::min;
using std::max;
using std::make_pair;
using boost::shared_ptr;

AudioMerger::AudioMerger (int channels, int frame_rate)
	: _buffers (new AudioBuffers (channels, 0))
	, _last_pull (0)
	, _frame_rate (frame_rate)
{

}

/** Pull audio up to a given time; after this call, no more data can be pushed
 *  before the specified time.
 */
pair<shared_ptr<AudioBuffers>, DCPTime>
AudioMerger::pull (DCPTime time)
{
	/* Number of frames to return */
	Frame const to_return = time.frames_floor (_frame_rate) - _last_pull.frames_floor (_frame_rate);
	shared_ptr<AudioBuffers> out (new AudioBuffers (_buffers->channels(), to_return));

	/* And this is how many we will get from our buffer */
	Frame const to_return_from_buffers = min (to_return, Frame (_buffers->frames()));

	/* Copy the data that we have to the back end of the return buffer */
	out->copy_from (_buffers.get(), to_return_from_buffers, 0, to_return - to_return_from_buffers);
	/* Silence any gap at the start */
	out->make_silent (0, to_return - to_return_from_buffers);

	DCPTime out_time = _last_pull;
	_last_pull = time;

	/* And remove the data we're returning from our buffers */
	if (_buffers->frames() > to_return_from_buffers) {
		_buffers->move (to_return_from_buffers, 0, _buffers->frames() - to_return_from_buffers);
	}
	_buffers->set_frames (_buffers->frames() - to_return_from_buffers);

	return make_pair (out, out_time);
}

void
AudioMerger::push (boost::shared_ptr<const AudioBuffers> audio, DCPTime time)
{
	DCPOMATIC_ASSERT (time >= _last_pull);

	Frame const frame = time.frames_floor (_frame_rate);
	Frame after = max (Frame (_buffers->frames()), frame + audio->frames() - _last_pull.frames_floor (_frame_rate));
	_buffers->ensure_size (after);
	_buffers->accumulate_frames (audio.get(), 0, frame - _last_pull.frames_floor (_frame_rate), audio->frames ());
	_buffers->set_frames (after);
}
