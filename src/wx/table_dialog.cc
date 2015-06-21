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

#include "table_dialog.h"
#include "wx_util.h"

TableDialog::TableDialog (wxWindow* parent, wxString title, int columns, bool cancel)
	: wxDialog (parent, wxID_ANY, title)
{
	_overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_overall_sizer);

	_table = new wxFlexGridSizer (columns, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_table->AddGrowableCol (1, 1);

	_overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	long int flags = wxOK;
	if (cancel) {
		flags |= wxCANCEL;
	}

	wxSizer* buttons = CreateSeparatedButtonSizer (flags);
	if (buttons) {
		_overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
}

void
TableDialog::layout ()
{
	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);
}

void
TableDialog::add (wxString text, bool label)
{
	add_label_to_sizer (_table, this, text, label);
}

void
TableDialog::add_spacer ()
{
	_table->AddSpacer (0);
}
