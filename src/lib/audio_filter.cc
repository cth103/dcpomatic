/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_filter.h"
#include "audio_buffers.h"
#include "util.h"
#include <cmath>


using std::make_shared;
using std::min;
using std::shared_ptr;


std::vector<float>
AudioFilter::sinc_blackman (float cutoff, bool invert) const
{
	auto ir = std::vector<float>();
	ir.reserve(_M + 1);

	/* Impulse response */

	for (int i = 0; i <= _M; ++i) {
		if (i == (_M / 2)) {
			ir[i] = 2 * M_PI * cutoff;
		} else {
			/* sinc */
			ir[i] = sin (2 * M_PI * cutoff * (i - _M / 2)) / (i - _M / 2);
			/* Blackman window */
			ir[i] *= (0.42 - 0.5 * cos (2 * M_PI * i / _M) + 0.08 * cos (4 * M_PI * i / _M));
		}
	}

	/* Normalise */

	float sum = 0;
	for (int i = 0; i <= _M; ++i) {
		sum += ir[i];
	}

	for (int i = 0; i <= _M; ++i) {
		ir[i] /= sum;
	}

	/* Frequency inversion (swapping low-pass for high-pass, or whatever) */

	if (invert) {
		for (int i = 0; i <= _M; ++i) {
			ir[i] = -ir[i];
		}
		ir[_M / 2] += 1;
	}

	return ir;
}


shared_ptr<AudioBuffers>
AudioFilter::run (shared_ptr<const AudioBuffers> in)
{
	auto out = make_shared<AudioBuffers>(in->channels(), in->frames());

	if (!_tail) {
		_tail.reset (new AudioBuffers (in->channels(), _M + 1));
		_tail->make_silent ();
	}

	int const channels = in->channels ();
	int const frames = in->frames ();

	for (int i = 0; i < channels; ++i) {
		auto tail_p = _tail->data (i);
		auto in_p = in->data (i);
		auto out_p = out->data (i);
		for (int j = 0; j < frames; ++j) {
			float s = 0;
			for (int k = 0; k <= _M; ++k) {
				if ((j - k) < 0) {
					s += tail_p[j - k + _M + 1] * _ir[k];
				} else {
					s += in_p[j - k] * _ir[k];
				}
			}

			out_p[j] = s;
		}
	}

	int const amount = min (in->frames(), _tail->frames());
	if (amount < _tail->frames ()) {
		_tail->move (_tail->frames() - amount, amount, 0);
	}
	_tail->copy_from (in.get(), amount, in->frames() - amount, _tail->frames () - amount);

	return out;
}


void
AudioFilter::flush ()
{
	_tail.reset ();
}


LowPassAudioFilter::LowPassAudioFilter (float transition_bandwidth, float cutoff)
	: AudioFilter (transition_bandwidth)
{
	_ir = sinc_blackman (cutoff, false);
}


HighPassAudioFilter::HighPassAudioFilter (float transition_bandwidth, float cutoff)
	: AudioFilter (transition_bandwidth)
{
	_ir = sinc_blackman (cutoff, true);
}


BandPassAudioFilter::BandPassAudioFilter (float transition_bandwidth, float lower, float higher)
	: AudioFilter (transition_bandwidth)
{
	auto lpf = sinc_blackman (lower, false);
	auto hpf = sinc_blackman (higher, true);

	_ir.reserve (_M + 1);
	for (int i = 0; i <= _M; ++i) {
		_ir[i] = lpf[i] + hpf[i];
	}

	/* We now have a band-stop, so invert for band-pass */
	for (int i = 0; i <= _M; ++i) {
		_ir[i] = -_ir[i];
	}

	_ir[_M / 2] += 1;
}
