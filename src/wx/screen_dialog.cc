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

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, optional<dcp::Certificate> recipient)
	: TableDialog (parent, std_to_wx (title), 2, 1, true)
	, _recipient (recipient)
{
	add (_("Name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1)));

	add (_("Recipient certificate"), true);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_recipient_thumbprint = new wxStaticText (this, wxID_ANY, wxT (""));
	wxFont font = _recipient_thumbprint->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	_recipient_thumbprint->SetFont (font);
	if (recipient) {
		_recipient_thumbprint->SetLabel (std_to_wx (recipient->thumbprint ()));
	}
	_get_recipient_from_file = new wxButton (this, wxID_ANY, _("Get from file..."));
	_download_recipient = new wxButton (this, wxID_ANY, _("Download..."));
	s->Add (_recipient_thumbprint, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	s->Add (_get_recipient_from_file, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	s->Add (_download_recipient, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	add (s);

	_get_recipient_from_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::get_recipient_from_file, this));
	_download_recipient->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::download_recipient, this));

	setup_sensitivity ();
	layout ();
}

string
ScreenDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

optional<dcp::Certificate>
ScreenDialog::recipient () const
{
	return _recipient;
}

void
ScreenDialog::load_recipient (boost::filesystem::path file)
{
	try {
		_recipient = dcp::Certificate (dcp::file_to_string (file));
		_recipient_thumbprint->SetLabel (std_to_wx (_recipient->thumbprint ()));
	} catch (dcp::MiscError& e) {
		error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), std_to_wx(e.what()).data()));
	}
}

void
ScreenDialog::get_recipient_from_file ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));
	if (d->ShowModal () == wxID_OK) {
		load_recipient (boost::filesystem::path (wx_to_std (d->GetPath ())));
	}
	d->Destroy ();

	setup_sensitivity ();
}

void
ScreenDialog::download_recipient ()
{
	DownloadCertificateDialog* d = new DownloadCertificateDialog (this);
	if (d->ShowModal() == wxID_OK) {
		_recipient = d->certificate ();
		_recipient_thumbprint->SetLabel (std_to_wx (_recipient->thumbprint ()));
	}
	d->Destroy ();
	setup_sensitivity ();
}

void
ScreenDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(_recipient));
	}
}
