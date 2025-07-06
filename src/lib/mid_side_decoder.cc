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
#include "mid_side_decoder.h"

#include "i18n.h"


using std::make_shared;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;


string
MidSideDecoder::name() const
{
	return _("Mid-side decoder");
}


string
MidSideDecoder::id() const
{
	return N_("mid-side-decoder");
}


int
MidSideDecoder::out_channels() const
{
	return 3;
}


shared_ptr<AudioProcessor>
MidSideDecoder::clone(int) const
{
	return make_shared<MidSideDecoder>();
}


shared_ptr<AudioBuffers>
MidSideDecoder::run(shared_ptr<const AudioBuffers> in, int channels)
{
	int const N = min(channels, 3);
	auto out = make_shared<AudioBuffers>(channels, in->frames());
	for (int i = 0; i < in->frames(); ++i) {
		auto const left = in->data()[0][i];
		auto const right = in->data()[1][i];
		auto const mid = (left + right) / 2;
		if (N > 0) {
			out->data()[0][i] = left - mid;
		}
		if (N > 1) {
			out->data()[1][i] = right - mid;
		}
		if (N > 2) {
			out->data()[2][i] = mid;
		}
	}

	return out;
}


void
MidSideDecoder::make_audio_mapping_default(AudioMapping& mapping) const
{
	/* Just map the first two input channels to our M/S */
	mapping.make_zero();
	for (int i = 0; i < min(2, mapping.input_channels()); ++i) {
		mapping.set(i, i, 1);
	}
}


vector<NamedChannel>
MidSideDecoder::input_names() const
{
	return {
		NamedChannel(_("Left"), 0),
		NamedChannel(_("Right"), 1)
	};
}
