/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "repeat_dialog.h"
#include "wx_util.h"

RepeatDialog::RepeatDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Repeat Content"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);
	
	wxFlexGridSizer* table = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);

	add_label_to_sizer (table, this, _("Repeat"), true);
	_number = new wxSpinCtrl (this, wxID_ANY);
	table->Add (_number, 1);

	add_label_to_sizer (table, this, _("times"), false);

	_number->SetRange (1, 1024);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

int
RepeatDialog::number () const
{
	return _number->GetValue ();
}
