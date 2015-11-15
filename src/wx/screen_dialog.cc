/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "screen_dialog.h"
#include "wx_util.h"
#include "download_certificate_dialog.h"
#include "lib/compose.hpp"
#include "lib/util.h"
#include <dcp/exceptions.h>
#include <wx/filepicker.h>
#include <wx/validate.h>
#include <iostream>

using std::string;
using std::cout;
using boost::optional;

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, optional<dcp::Certificate> certificate)
	: TableDialog (parent, std_to_wx (title), 2, 1, true)
	, _certificate (certificate)
{
	add (_("Name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1)));

	add (_("Certificate"), true);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_certificate_thumbprint = new wxStaticText (this, wxID_ANY, wxT (""));
	wxFont font = _certificate_thumbprint->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	_certificate_thumbprint->SetFont (font);
	if (certificate) {
		_certificate_thumbprint->SetLabel (std_to_wx (certificate->thumbprint ()));
	}
	_load_certificate = new wxButton (this, wxID_ANY, _("Load from file..."));
	_download_certificate = new wxButton (this, wxID_ANY, _("Download..."));
	s->Add (_certificate_thumbprint, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	s->Add (_load_certificate, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	s->Add (_download_certificate, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	add (s);

	_load_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::select_certificate, this));
	_download_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::download_certificate, this));

	setup_sensitivity ();
	layout ();
}

string
ScreenDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

optional<dcp::Certificate>
ScreenDialog::certificate () const
{
	return _certificate;
}

void
ScreenDialog::load_certificate (boost::filesystem::path file)
{
	try {
		_certificate = dcp::Certificate (dcp::file_to_string (file));
		_certificate_thumbprint->SetLabel (std_to_wx (_certificate->thumbprint ()));
	} catch (dcp::MiscError& e) {
		error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), std_to_wx(e.what()).data()));
	}
}

void
ScreenDialog::select_certificate ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));
	if (d->ShowModal () == wxID_OK) {
		load_certificate (boost::filesystem::path (wx_to_std (d->GetPath ())));
	}
	d->Destroy ();

	setup_sensitivity ();
}

void
ScreenDialog::download_certificate ()
{
	DownloadCertificateDialog* d = new DownloadCertificateDialog (this);
	if (d->ShowModal() == wxID_OK) {
		_certificate = d->certificate ();
		_certificate_thumbprint->SetLabel (std_to_wx (_certificate->thumbprint ()));
	}
	d->Destroy ();
	setup_sensitivity ();
}

void
ScreenDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(_certificate));
	}
}
