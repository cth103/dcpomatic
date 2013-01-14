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

#include <wx/filepicker.h>
#include <wx/validate.h>
#include <libdcp/exceptions.h>
#include "lib/compose.hpp"
#include "screen_dialog.h"
#include "wx_util.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, shared_ptr<libdcp::Certificate> certificate)
	: wxDialog (parent, wxID_ANY, std_to_wx (title))
	, _certificate (certificate)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, "Name");
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1));
	table->Add (_name, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Certificate");
	_certificate_load = new wxButton (this, wxID_ANY, wxT ("Load from file..."));
	table->Add (_certificate_load, 1, wxEXPAND);

	table->AddSpacer (0);
	_certificate_text = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (320, 256), wxTE_MULTILINE | wxTE_READONLY);
	if (certificate) {
		_certificate_text->SetValue (certificate->certificate ());
	}
	wxFont font = wxSystemSettings::GetFont (wxSYS_ANSI_FIXED_FONT);
	font.SetPointSize (font.GetPointSize() / 2);
	_certificate_text->SetFont (font);
	table->Add (_certificate_text, 1, wxEXPAND);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_certificate_load->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (ScreenDialog::load_certificate), 0, this);
}

string
ScreenDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

shared_ptr<libdcp::Certificate>
ScreenDialog::certificate () const
{
	return _certificate;
}

void
ScreenDialog::load_certificate (wxCommandEvent &)
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));
	d->ShowModal ();
	
	try {
		_certificate.reset (new libdcp::Certificate (wx_to_std (d->GetPath ())));
		_certificate_text->SetValue (_certificate->certificate ());
	} catch (libdcp::MiscError& e) {
		error_dialog (this, String::compose ("Could not read certificate file (%1)", e.what()));
	}

	d->Destroy ();
}
