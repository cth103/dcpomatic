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
	: wxDialog (parent, wxID_ANY, _("Channel gain"))
{
	wxFlexGridSizer* table = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, wxString::Format (_("Gain for content channel %d in DCP channel %d"), c + 1, d + 1), false);
	_gain = new wxSpinCtrlDouble (this);
	table->Add (_gain);

	add_label_to_sizer (table, this, _("dB"), false);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_gain->SetRange (-144, 0);
	_gain->SetDigits (1);
	_gain->SetIncrement (0.1);

	_gain->SetValue (20 * log10 (v));
}

float
AudioGainDialog::value () const
{
	if (_gain->GetValue() <= -144) {
		return 0;
	}
	
	return pow (10, _gain->GetValue () / 20);
}
