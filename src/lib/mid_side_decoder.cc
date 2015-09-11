/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "mid_side_decoder.h"
#include "audio_buffers.h"
#include "audio_mapping.h"

#include "i18n.h"

using std::string;
using std::min;
using std::vector;
using boost::shared_ptr;

string
MidSideDecoder::name () const
{
	return _("Mid-side decoder");
}

string
MidSideDecoder::id () const
{
	return N_("mid-side-decoder");
}

ChannelCount
MidSideDecoder::in_channels () const
{
	return ChannelCount (2);
}

int
MidSideDecoder::out_channels () const
{
	return 3;
}

shared_ptr<AudioProcessor>
MidSideDecoder::clone (int) const
{
	return shared_ptr<AudioProcessor> (new MidSideDecoder ());
}

shared_ptr<AudioBuffers>
MidSideDecoder::run (shared_ptr<const AudioBuffers> in, int channels)
{
	int const N = min (channels, 3);
	shared_ptr<AudioBuffers> out (new AudioBuffers (channels, in->frames ()));
	for (int i = 0; i < in->frames(); ++i) {
		float const left = in->data()[0][i];
		float const right = in->data()[1][i];
		float const mid = (left + right) / 2;
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

	for (int i = N; i < channels; ++i) {
		out->make_silent (i);
	}

	return out;
}

void
MidSideDecoder::make_audio_mapping_default (AudioMapping& mapping) const
{
	/* Just map the first two input channels to our M/S */
	mapping.make_zero ();
	for (int i = 0; i < min (2, mapping.input_channels()); ++i) {
		mapping.set (i, i, 1);
	}
}

vector<string>
MidSideDecoder::input_names () const
{
	vector<string> n;

	n.push_back (_("Left"));
	n.push_back (_("Right"));

	return n;
}
