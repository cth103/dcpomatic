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

#include <cmath>
#include <wx/spinctrl.h>
#include "audio_gain_dialog.h"
#include "wx_util.h"

AudioGainDialog::AudioGainDialog (wxWindow* parent, int c, int d, float v)
	: TableDialog (parent, _("Channel gain"), 3, true)
{
	add (wxString::Format (_("Gain for content channel %d in DCP channel %d"), c + 1, d + 1), false);
	_gain = add (new wxSpinCtrlDouble (this));
	add (_("dB"), false);

	_gain->SetRange (-144, 0);
	_gain->SetDigits (1);
	_gain->SetIncrement (0.1);

	_gain->SetValue (20 * log10 (v));

	layout ();
}

float
AudioGainDialog::value () const
{
	if (_gain->GetValue() <= -144) {
		return 0;
	}

	return pow (10, _gain->GetValue () / 20);
}
