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

#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include "delay_line.h"
#include "util.h"

using std::min;
using boost::shared_ptr;

/** @param channels Number of channels of audio.
 *  @param frames Delay in frames, +ve to move audio later.
 */
DelayLine::DelayLine (Log* log, int channels, int frames)
	: AudioProcessor (log)
	, _negative_delay_remaining (0)
	, _frames (frames)
{
	if (_frames > 0) {
		/* We need a buffer to keep some data in */
		_buffers.reset (new AudioBuffers (channels, _frames));
		_buffers->make_silent ();
	} else if (_frames < 0) {
		/* We can do -ve delays just by chopping off
		   the start, so no buffer needed.
		*/
		_negative_delay_remaining = -_frames;
	}
}

void
DelayLine::process_audio (shared_ptr<AudioBuffers> data)
{
	if (_buffers) {
		/* We have some buffers, so we are moving the audio later */

		/* Copy the input data */
		AudioBuffers input (*data.get ());

		int to_do = data->frames ();

		/* Write some of our buffer to the output */
		int const from_buffer = min (to_do, _buffers->frames());
		data->copy_from (_buffers.get(), from_buffer, 0, 0);
		to_do -= from_buffer;

		/* Write some of the input to the output */
		int const from_input = to_do;
		data->copy_from (&input, from_input, 0, from_buffer);

		int const left_in_buffer = _buffers->frames() - from_buffer;

		/* Shuffle our buffer down */
		_buffers->move (from_buffer, 0, left_in_buffer);

		/* Copy remaining input data to our buffer */
		_buffers->copy_from (&input, input.frames() - from_input, from_input, left_in_buffer);

	} else {

		/* Chop the initial data off until _negative_delay_remaining
		   is zero, then just pass data.
		*/

		int const to_do = min (data->frames(), _negative_delay_remaining);
		if (to_do) {
			data->move (to_do, 0, data->frames() - to_do);
			data->set_frames (data->frames() - to_do);
			_negative_delay_remaining -= to_do;
		}
	}

	Audio (data);
}
