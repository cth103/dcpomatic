/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_FILTER_H
#define DCPOMATIC_AUDIO_FILTER_H

#include <boost/shared_ptr.hpp>

class AudioBuffers;
struct audio_filter_impulse_input_test;

/** An audio filter which can take AudioBuffers and apply some filtering operation,
 *  returning filtered samples
 */
class AudioFilter
{
public:
	AudioFilter (float transition_bandwidth)
		: _ir (0)
	{
		_M = 4 / transition_bandwidth;
		if (_M % 2) {
			++_M;
		}
	}

	virtual ~AudioFilter ();

	boost::shared_ptr<AudioBuffers> run (boost::shared_ptr<const AudioBuffers> in);

	void flush ();

protected:
	friend struct audio_filter_impulse_kernel_test;
	friend struct audio_filter_impulse_input_test;

	float* sinc_blackman (float cutoff, bool invert) const;

	float* _ir;
	int _M;
	boost::shared_ptr<AudioBuffers> _tail;
};

class LowPassAudioFilter : public AudioFilter
{
public:
	/** Construct a windowed-sinc low-pass filter using the Blackman window.
	 *  @param transition_bandwidth Transition bandwidth as a fraction of the sampling rate.
	 *  @param cutoff Cutoff frequency as a fraction of the sampling rate.
	 */
	LowPassAudioFilter (float transition_bandwidth, float cutoff);
};

class HighPassAudioFilter : public AudioFilter
{
public:
	/** Construct a windowed-sinc high-pass filter using the Blackman window.
	 *  @param transition_bandwidth Transition bandwidth as a fraction of the sampling rate.
	 *  @param cutoff Cutoff frequency as a fraction of the sampling rate.
	 */
	HighPassAudioFilter (float transition_bandwidth, float cutoff);
};

class BandPassAudioFilter : public AudioFilter
{
public:
	/** Construct a windowed-sinc band-pass filter using the Blackman window.
	 *  @param transition_bandwidth Transition bandwidth as a fraction of the sampling rate.
	 *  @param lower Lower cutoff frequency as a fraction of the sampling rate.
	 *  @param higher Higher cutoff frequency as a fraction of the sampling rate.
	 */
	BandPassAudioFilter (float transition_bandwidth, float lower, float higher);
};

#endif
