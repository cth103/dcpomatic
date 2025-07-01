/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "upmixer_b.h"
#include "audio_buffers.h"
#include "audio_mapping.h"

#include "i18n.h"


using std::string;
using std::make_shared;
using std::min;
using std::vector;
using std::shared_ptr;


UpmixerB::UpmixerB(int sampling_rate)
	: _lfe(0.01, 150.0 / sampling_rate)
	, _delay(0.02 * sampling_rate)
{

}


string
UpmixerB::name() const
{
	return _("Stereo to 5.1 up-mixer B");
}


string
UpmixerB::id() const
{
	return N_("stereo-5.1-upmix-b");
}


int
UpmixerB::out_channels() const
{
	return 6;
}


shared_ptr<AudioProcessor>
UpmixerB::clone(int sampling_rate) const
{
	return make_shared<UpmixerB>(sampling_rate);
}


shared_ptr<AudioBuffers>
UpmixerB::run(shared_ptr<const AudioBuffers> in, int channels)
{
	auto out = make_shared<AudioBuffers>(channels, in->frames());

	/* L + R minus 6dB (in terms of amplitude) */
	auto in_LR = in->channel(0);
	in_LR->accumulate_frames(in->channel(1).get(), in->frames(), 0, 0);
	in_LR->apply_gain(-6);

	if (channels > 0) {
		/* L = Lt */
		out->copy_channel_from(in.get(), 0, 0);
	}

	if (channels > 1) {
		/* R = Rt */
		out->copy_channel_from(in.get(), 1, 1);
	}

	if (channels > 2) {
		/* C = L + R minus 3dB */
		out->copy_channel_from(in_LR.get(), 0, 2);
	}

	if (channels > 3) {
		/* Lfe is filtered C */
		out->copy_channel_from(_lfe.run(in_LR).get(), 0, 3);
	}

	shared_ptr<AudioBuffers> S;
	if (channels > 4) {
		/* Ls is L - R with some delay */
		auto sub = make_shared<AudioBuffers>(1, in->frames());
		sub->copy_channel_from(in.get(), 0, 0);
		float* p = sub->data(0);
		float const * q = in->data(1);
		for (int i = 0; i < in->frames(); ++i) {
			*p++ -= *q++;
		}
		S = _delay.run(sub);
		out->copy_channel_from(S.get(), 0, 4);
	}

	if (channels > 5) {
		/* Rs = Ls */
		out->copy_channel_from(S.get(), 0, 5);
	}

	return out;
}


void
UpmixerB::flush()
{
	_lfe.flush();
	_delay.flush();
}


void
UpmixerB::make_audio_mapping_default(AudioMapping& mapping) const
{
	/* Just map the first two input channels to our L/R */
	mapping.make_zero();
	for (int i = 0; i < min(2, mapping.input_channels()); ++i) {
		mapping.set(i, i, 1);
	}
}


vector<NamedChannel>
UpmixerB::input_names() const
{
	return {
		NamedChannel(_("Upmix L"), 0),
		NamedChannel(_("Upmix R"), 1)
	};
}
