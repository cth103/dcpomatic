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

	_n_frames->SetRange (1, INT_MAX - 1);
	if (_film->dcp_frames()) {
		_whole->SetValue (false);
		_first->SetValue (true);
		_n_frames->SetValue (_film->dcp_frames().get());
	} else {
		_whole->SetValue (true);
		_first->SetValue (false);
		_n_frames->SetValue (24);
	}

	_whole->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (DCPRangeDialog::whole_toggled), 0, this);
	_first->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (DCPRangeDialog::first_toggled), 0, this);
	_n_frames->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (DCPRangeDialog::n_frames_changed), 0, this);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 0, wxALL, 6);
	
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

	Changed (frames);
}
