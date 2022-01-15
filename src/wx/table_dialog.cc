/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "wx_util.h"
#include "static_text.h"

TableDialog::TableDialog (wxWindow* parent, wxString title, int columns, int growable, bool cancel)
	: wxDialog (parent, wxID_ANY, title)
{
	_overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_overall_sizer);

	_table = new wxFlexGridSizer (columns, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_table->AddGrowableCol (growable, 1);

	_overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	long int flags = wxOK;
	if (cancel) {
		flags |= wxCANCEL;
	}

	auto buttons = CreateSeparatedButtonSizer (flags);
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

wxStaticText *
#ifdef DCPOMATIC_OSX
TableDialog::add (wxString text, bool label)
#else
TableDialog::add (wxString text, bool)
#endif
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT;
#ifdef DCPOMATIC_OSX
	if (label) {
		flags |= wxALIGN_RIGHT;
		text += wxT (":");
	}
#endif
	auto m = new StaticText (this, wxT (""));
	m->SetLabelMarkup (text);
	_table->Add (m, 0, flags, 6);
	return m;
}

void
TableDialog::add_spacer ()
{
	_table->AddSpacer (0);
}
