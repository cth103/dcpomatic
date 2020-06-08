/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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

#include "audio_gain_dialog.h"
#include "wx_util.h"
#include "lib/util.h"
#include <cmath>
#include <wx/spinctrl.h>

AudioGainDialog::AudioGainDialog (wxWindow* parent, int c, int d, float v)
	: TableDialog (parent, _("Channel gain"), 3, 1, true)
{
	add (wxString::Format (_("Gain for content channel %d in DCP channel %d"), c + 1, d + 1), false);
	_gain = add (new wxSpinCtrlDouble (this));
	add (_("dB"), false);

	_gain->SetRange (-144, 18);
	_gain->SetDigits (1);
	_gain->SetIncrement (0.1);

	_gain->SetValue (linear_to_db(v));

	layout ();

	_gain->SetFocus ();
}

float
AudioGainDialog::value () const
{
	if (_gain->GetValue() <= -144) {
		return 0;
	}

	return db_to_linear (_gain->GetValue());
}
