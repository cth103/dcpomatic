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

#include "lib/film.h"
#include "dcp_range_dialog.h"
#include "wx_util.h"

using boost::shared_ptr;

DCPRangeDialog::DCPRangeDialog (wxWindow* p, shared_ptr<Film> f)
	: wxDialog (p, wxID_ANY, wxString (_("DCP Range")))
	, _film (f)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (3, 6, 6);

	add_label_to_sizer (table, this, "Trim start");
	_trim_start = new wxSpinCtrl (this, wxID_ANY);
	table->Add (_trim_start, 1);
	add_label_to_sizer (table, this, "frames");

	add_label_to_sizer (table, this, "Trim end");
	_trim_end = new wxSpinCtrl (this, wxID_ANY);
	table->Add (_trim_end, 1);
	add_label_to_sizer (table, this, "frames");

	_trim_start->SetValue (_film->dcp_trim_start());
	_trim_end->SetValue (_film->dcp_trim_end());

	_trim_start->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (DCPRangeDialog::emit_changed), 0, this);
	_trim_end->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (DCPRangeDialog::emit_changed), 0, this);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 0, wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->SetSizeHints (this);
}

void
DCPRangeDialog::emit_changed (wxCommandEvent &)
{
	Changed (_trim_start->GetValue(), _trim_end->GetValue());
}
