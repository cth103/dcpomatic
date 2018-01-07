/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "nag_dialog.h"
#include "wx_util.h"
#include <wx/richtext/richtextctrl.h>
#include <boost/foreach.hpp>

using boost::shared_ptr;

NagDialog::NagDialog (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel)
	: wxDialog (parent, wxID_ANY, _("Important notice"))
	, _nag (nag)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	_text = new wxStaticText (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300));
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	wxCheckBox* b = new wxCheckBox (this, wxID_ANY, _("Don't show this message again"));
	sizer->Add (b, 0, wxALL, 6);
	b->Bind (wxEVT_CHECKBOX, bind (&NagDialog::shut_up, this, _1));

	int flags = wxOK;
	if (can_cancel) {
		flags |= wxCANCEL;
	}
	wxSizer* buttons = CreateSeparatedButtonSizer (flags);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->SetLabelMarkup (message);
}

void
NagDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_nagged (_nag, ev.IsChecked());
}

/** @return true if the user clicked Cancel */
bool
NagDialog::maybe_nag (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel)
{
	if (Config::instance()->nagged(nag)) {
		return false;
	}

	NagDialog* d = new NagDialog (parent, nag, message, can_cancel);
	int const r = d->ShowModal();
	d->Destroy ();

	return r == wxID_CANCEL;
}
