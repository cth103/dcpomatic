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


#include "audio_buffers.h"
#include "audio_mapping.h"
#include "upmixer_a.h"

#include "i18n.h"


using std::make_shared;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;


UpmixerA::UpmixerA (int sampling_rate)
	: _left (0.02, 1900.0 / sampling_rate, 4800.0 / sampling_rate)
	, _right (0.02, 1900.0 / sampling_rate, 4800.0 / sampling_rate)
	, _centre (0.01, 150.0 / sampling_rate, 1900.0 / sampling_rate)
	, _lfe (0.01, 150.0 / sampling_rate)
	, _ls (0.02, 4800.0 / sampling_rate, 20000.0 / sampling_rate)
	, _rs (0.02, 4800.0 / sampling_rate, 20000.0 / sampling_rate)
{

}


string
UpmixerA::name () const
{
	return _("Stereo to 5.1 up-mixer A");
}


string
UpmixerA::id () const
{
	return N_("stereo-5.1-upmix-a");
}


int
UpmixerA::out_channels () const
{
	return 6;
}


shared_ptr<AudioProcessor>
UpmixerA::clone (int sampling_rate) const
{
	return make_shared<UpmixerA>(sampling_rate);
}


shared_ptr<AudioBuffers>
UpmixerA::run (shared_ptr<const AudioBuffers> in, int channels)
{
	/* Input L and R */
	auto in_L = in->channel (0);
	auto in_R = in->channel (1);

	/* Mix of L and R; -6dB down in amplitude (3dB in terms of power) */
	auto in_LR = in_L->clone ();
	in_LR->accumulate_frames (in_R.get(), in_R->frames(), 0, 0);
	in_LR->apply_gain (-6);

	/* Run filters */
	vector<shared_ptr<AudioBuffers>> all_out = {
		_left.run(in_L),
		_right.run(in_R),
		_centre.run(in_LR),
		_lfe.run(in_LR),
		_ls.run(in_L),
		_rs.run(in_R)
	};

	auto out = make_shared<AudioBuffers>(channels, in->frames());
	int const N = min (channels, 6);

	for (int i = 0; i < N; ++i) {
		out->copy_channel_from (all_out[i].get(), 0, i);
	}

	for (int i = N; i < channels; ++i) {
		out->make_silent (i);
	}

	return out;
}

void
UpmixerA::flush ()
{
	_left.flush ();
	_right.flush ();
	_centre.flush ();
	_lfe.flush ();
	_ls.flush ();
	_rs.flush ();
}


void
UpmixerA::make_audio_mapping_default (AudioMapping& mapping) const
{
	/* Just map the first two input channels to our L/R */
	mapping.make_zero ();
	for (int i = 0; i < min (2, mapping.input_channels()); ++i) {
		mapping.set (i, i, 1);
	}
}


vector<NamedChannel>
UpmixerA::input_names () const
{
	return {
		NamedChannel(_("Upmix L"), 0),
		NamedChannel(_("Upmix R"), 1)
	};
}
