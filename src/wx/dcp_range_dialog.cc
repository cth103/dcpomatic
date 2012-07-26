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

DCPRangeDialog::DCPRangeDialog (wxWindow* p, Film* f)
	: wxDialog (p, wxID_ANY, wxString (_("DCP Range")))
	, _film (f)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);

	_whole = new wxRadioButton (this, wxID_ANY, _("Whole film"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	table->Add (_whole, 1);
	table->AddSpacer (0);
	
	_first = new wxRadioButton (this, wxID_ANY, _("First"));
	table->Add (_first);
	{
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_n_frames = new wxSpinCtrl (this, wxID_ANY);
		s->Add (_n_frames);
		add_label_to_sizer (s, this, "frames");
		table->Add (s);
	}

	table->AddSpacer (0);
	_cut = new wxRadioButton (this, wxID_ANY, _("Cut remainder"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	table->Add (_cut);

	table->AddSpacer (0);
	_black_out = new wxRadioButton (this, wxID_ANY, _("Black-out remainder"));
	table->Add (_black_out);

	_n_frames->SetRange (1, INT_MAX - 1);
	if (_film->dcp_frames() > 0) {
		_whole->SetValue (false);
		_first->SetValue (true);
		_n_frames->SetValue (_film->dcp_frames ());
	} else {
		_whole->SetValue (true);
		_first->SetValue (false);
		_n_frames->SetValue (24);
	}

	_black_out->Enable (_film->dcp_trim_action() == BLACK_OUT);
	_cut->Enable (_film->dcp_trim_action() == CUT);

	_whole->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (DCPRangeDialog::whole_toggled), 0, this);
	_first->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (DCPRangeDialog::first_toggled), 0, this);
	_cut->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (DCPRangeDialog::cut_toggled), 0, this);
	_n_frames->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (DCPRangeDialog::n_frames_changed), 0, this);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->SetSizeHints (this);

	set_sensitivity ();
}

void
DCPRangeDialog::whole_toggled (wxCommandEvent &)
{
	set_sensitivity ();
	emit_changed ();
}

void
DCPRangeDialog::first_toggled (wxCommandEvent &)
{
	set_sensitivity ();
	emit_changed ();
}

void
DCPRangeDialog::set_sensitivity ()
{
	_n_frames->Enable (_first->GetValue ());
	_black_out->Enable (_first->GetValue ());
	_cut->Enable (_first->GetValue ());
}

void
DCPRangeDialog::cut_toggled (wxCommandEvent &)
{
	set_sensitivity ();
	emit_changed ();
}

void
DCPRangeDialog::n_frames_changed (wxCommandEvent &)
{
	emit_changed ();
}

void
DCPRangeDialog::emit_changed ()
{
	int frames = 0;
	if (!_whole->GetValue ()) {
		frames = _n_frames->GetValue ();
	}

	TrimAction action = CUT;
	if (_black_out->GetValue ()) {
		action = BLACK_OUT;
	}

	Changed (frames, action);
}
