/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "util.h"

template <class T, class F>
class AudioMerger
{
public:
	AudioMerger (int channels, boost::function<F (T)> t_to_f, boost::function<T (F)> f_to_t)
		: _buffers (new AudioBuffers (channels, 0))
		, _last_pull (0)
		, _t_to_f (t_to_f)
		, _f_to_t (f_to_t)
	{}

	/** Pull audio up to a given time; after this call, no more data can be pushed
	 *  before the specified time.
	 */
	TimedAudioBuffers<T>
	pull (T time)
	{
		TimedAudioBuffers<T> out;
		
		F const to_return = _t_to_f (time - _last_pull);
		out.audio.reset (new AudioBuffers (_buffers->channels(), to_return));
		/* And this is how many we will get from our buffer */
		F const to_return_from_buffers = min (to_return, _buffers->frames ());
		
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
	push (boost::shared_ptr<const AudioBuffers> audio, T time)
	{
		assert (time >= _last_pull);

		F frame = _t_to_f (time);
		F after = max (_buffers->frames(), frame + audio->frames() - _t_to_f (_last_pull));
		_buffers->ensure_size (after);
		_buffers->accumulate_frames (audio.get(), 0, frame - _t_to_f (_last_pull), audio->frames ());
		_buffers->set_frames (after);
	}

	F min (F a, int b)
	{
		if (a < b) {
			return a;
		}

		return b;
	}

	F max (int a, F b)
	{
		if (a > b) {
			return a;
		}

		return b;
	}
		
	TimedAudioBuffers<T>
	flush ()
	{
		if (_buffers->frames() == 0) {
			return TimedAudioBuffers<T> ();
		}

		return TimedAudioBuffers<T> (_buffers, _last_pull);
	}

	void
	clear (DCPTime t)
	{
		_last_pull = t;
		_buffers.reset (new AudioBuffers (_buffers->channels(), 0));
	}
	
private:
	boost::shared_ptr<AudioBuffers> _buffers;
	T _last_pull;
	boost::function<F (T)> _t_to_f;
	boost::function<T (F)> _f_to_t;
};
