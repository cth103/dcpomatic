/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "audio_buffers.h"
#include "audio_merger.h"

using std::min;
using std::max;
using boost::shared_ptr;

AudioMerger::AudioMerger (int channels, int frame_rate)
	: _buffers (new AudioBuffers (channels, 0))
	, _frame_rate (frame_rate)
	, _last_pull (0)
{

}


TimedAudioBuffers
AudioMerger::pull (DCPTime time)
{
	assert (time >= _last_pull);
	
	TimedAudioBuffers out;
	
	int64_t const to_return = DCPTime (time - _last_pull).frames (_frame_rate);
	out.audio.reset (new AudioBuffers (_buffers->channels(), to_return));
	/* And this is how many we will get from our buffer */
	int64_t const to_return_from_buffers = min (to_return, int64_t (_buffers->frames ()));
	
	/* Copy the data that we have to the back end of the return buffer */
	out.audio->copy_from (_buffers.get(), to_return_from_buffers, 0, to_return - to_return_from_buffers);
	/* Silence any gap at the start */
	out.audio->make_silent (0, to_return - to_return_from_buffers);
	
	out.time = _last_pull;
	_last_pull = time;
	
	/* And remove the data we're returning from our buffers */
	if (_buffers->frames() > to_return_from_buffers) {
		_buffers->move (to_return_from_buffers, 0, _buffers->frames() - to_return_from_buffers);
	}
	_buffers->set_frames (_buffers->frames() - to_return_from_buffers);
	
	return out;
}

void
AudioMerger::push (shared_ptr<const AudioBuffers> audio, DCPTime time)
{
	assert (time >= _last_pull);
	
	int64_t frame = time.frames (_frame_rate);
	int64_t after = max (int64_t (_buffers->frames()), frame + audio->frames() - _last_pull.frames (_frame_rate));
	_buffers->ensure_size (after);
	_buffers->accumulate_frames (audio.get(), 0, frame - _last_pull.frames (_frame_rate), audio->frames ());
	_buffers->set_frames (after);
}

TimedAudioBuffers
AudioMerger::flush ()
{
	if (_buffers->frames() == 0) {
		return TimedAudioBuffers ();
	}
	
	return TimedAudioBuffers (_buffers, _last_pull);
}

void
AudioMerger::clear (DCPTime t)
{
	_last_pull = t;
	_buffers.reset (new AudioBuffers (_buffers->channels(), 0));
}
