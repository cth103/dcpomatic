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

template <class T, class F>
class AudioMerger
{
public:
	AudioMerger (int channels, boost::function<F (T)> t_to_f, boost::function<T (F)> f_to_t)
		: _buffers (new AudioBuffers (channels, 0))
		, _next_emission (0)
		, _t_to_f (t_to_f)
		, _f_to_t (f_to_t)
	{}

	void push (boost::shared_ptr<const AudioBuffers> audio, T time)
	{
		if (time > _next_emission) {
			/* We can emit some audio from our buffer; this is how many frames
			   we are going to emit.
			*/
			F const to_emit = _t_to_f (time - _next_emission);
			boost::shared_ptr<AudioBuffers> emit (new AudioBuffers (_buffers->channels(), to_emit));

			/* And this is how many we will get from our buffer */
			F const to_emit_from_buffers = min (to_emit, _buffers->frames ());

			/* Copy the data that we have to the back end of `emit' */
			emit->copy_from (_buffers.get(), to_emit_from_buffers, 0, to_emit - to_emit_from_buffers);

			/* Silence any gap at the start */
			emit->make_silent (0, to_emit - to_emit_from_buffers);

			/* Emit that */
			Audio (emit, _next_emission);

			_next_emission += _f_to_t (to_emit);

			/* And remove the data we've emitted from our buffers */
			if (_buffers->frames() > to_emit_from_buffers) {
				_buffers->move (to_emit_from_buffers, 0, _buffers->frames() - to_emit_from_buffers);
			}
			_buffers->set_frames (_buffers->frames() - to_emit_from_buffers);
		}

		/* Now accumulate the new audio into our buffers */
		F frame = _t_to_f (time);
		F after = max (_buffers->frames(), frame + audio->frames() - _t_to_f (_next_emission));
		_buffers->ensure_size (after);
		_buffers->accumulate_frames (audio.get(), 0, frame - _t_to_f (_next_emission), audio->frames ());
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
		
	void flush ()
	{
		if (_buffers->frames() > 0) {
			Audio (_buffers, _next_emission);
		}
	}
	
	boost::signals2::signal<void (boost::shared_ptr<const AudioBuffers>, T)> Audio;

private:
	boost::shared_ptr<AudioBuffers> _buffers;
	T _next_emission;
	boost::function<F (T)> _t_to_f;
	boost::function<T (F)> _f_to_t;
};
