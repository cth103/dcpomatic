/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <boost/lexical_cast.hpp>
#include "gain_calculator_dialog.h"
#include "wx_util.h"

using namespace boost;

GainCalculatorDialog::GainCalculatorDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Gain Calculator"))
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, _("I want to play this back at fader"));
	_wanted = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, wxTextValidator (wxFILTER_NUMERIC));
	table->Add (_wanted, 1, wxEXPAND);

	add_label_to_sizer (table, this, _("But I have to use fader"));
	_actual = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, wxTextValidator (wxFILTER_NUMERIC));
	table->Add (_actual, 1, wxEXPAND);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
	
	SetSizer (overall_sizer);
	overall_sizer->SetSizeHints (this);
}

float
GainCalculatorDialog::wanted_fader () const
{
	if (_wanted->GetValue().IsEmpty()) {
		return 0;
	}
	
	return lexical_cast<float> (wx_to_std (_wanted->GetValue ()));
}

float
GainCalculatorDialog::actual_fader () const
{
	if (_actual->GetValue().IsEmpty()) {
		return 0;
	}

	return lexical_cast<float> (wx_to_std (_actual->GetValue ()));
}
