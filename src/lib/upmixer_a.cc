/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "upmixer_a.h"
#include "audio_buffers.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

UpmixerA::UpmixerA (int sampling_rate)
	: _left (0.02, 1900.0 / sampling_rate, 4800.0 / sampling_rate)
	, _right (0.02, 1900.0 / sampling_rate, 4800.0 / sampling_rate)
	, _centre (0.02, 150.0 / sampling_rate, 1900.0 / sampling_rate)
	, _lfe (0.02, 20.0 / sampling_rate, 150.0 / sampling_rate)
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

ChannelCount
UpmixerA::in_channels () const
{
	return ChannelCount (2);
}

int
UpmixerA::out_channels (int) const
{
	return 6;
}

shared_ptr<AudioProcessor>
UpmixerA::clone (int sampling_rate) const
{
	return shared_ptr<AudioProcessor> (new UpmixerA (sampling_rate));
}

shared_ptr<AudioBuffers>
UpmixerA::run (shared_ptr<const AudioBuffers> in)
{
	/* Input L and R */
	shared_ptr<AudioBuffers> in_L = in->channel (0);
	shared_ptr<AudioBuffers> in_R = in->channel (1);

	/* Mix of L and R */
	shared_ptr<AudioBuffers> in_LR = in_L->clone ();
	in_LR->accumulate_frames (in_R.get(), 0, 0, in_R->frames ());
	in_LR->apply_gain (0.5);

	/* Run filters */
	shared_ptr<AudioBuffers> L = _left.run (in_L);
	shared_ptr<AudioBuffers> R = _right.run (in_R);
	shared_ptr<AudioBuffers> C = _centre.run (in_LR);
	shared_ptr<AudioBuffers> Lfe = _lfe.run (in_LR);
	shared_ptr<AudioBuffers> Ls = _ls.run (in_L);
	shared_ptr<AudioBuffers> Rs = _rs.run (in_R);

	shared_ptr<AudioBuffers> out (new AudioBuffers (6, in->frames ()));
	out->copy_channel_from (L.get(), 0, 0);
	out->copy_channel_from (R.get(), 0, 1);
	out->copy_channel_from (C.get(), 0, 2);
	out->copy_channel_from (Lfe.get(), 0, 3);
	out->copy_channel_from (Ls.get(), 0, 4);
	out->copy_channel_from (Rs.get(), 0, 5);
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
