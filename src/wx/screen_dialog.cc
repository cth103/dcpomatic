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

#include <wx/filepicker.h>
#include <wx/validate.h>
#include <dcp/exceptions.h>
#include "lib/compose.hpp"
#include "lib/util.h"
#include "screen_dialog.h"
#include "wx_util.h"
#include "doremi_certificate_dialog.h"
#include "dolby_certificate_dialog.h"

using std::string;
using std::cout;
using boost::optional;

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, optional<dcp::Certificate> certificate)
	: TableDialog (parent, std_to_wx (title), 2, true)
	, _certificate (certificate)
{
	add (_("Name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1)));

	add (_("Server manufacturer"), true);
	_manufacturer = add (new wxChoice (this, wxID_ANY));

	add (_("Certificate"), true);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_load_certificate = new wxButton (this, wxID_ANY, _("Load from file..."));
	_download_certificate = new wxButton (this, wxID_ANY, _("Download"));
	s->Add (_load_certificate, 1, wxEXPAND);
	s->Add (_download_certificate, 1, wxEXPAND);
	add (s);

	add_spacer ();
	_certificate_text = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (320, 256), wxTE_MULTILINE | wxTE_READONLY);
	if (certificate) {
		_certificate_text->SetValue (certificate->certificate ());
	}
	wxFont font = wxSystemSettings::GetFont (wxSYS_ANSI_FIXED_FONT);
	font.SetPointSize (font.GetPointSize() / 2);
	_certificate_text->SetFont (font);
	add (_certificate_text);

	_manufacturer->Append (_("Unknown"));
	_manufacturer->Append (_("Doremi"));
	_manufacturer->Append (_("Dolby"));
	_manufacturer->Append (_("Other"));
	_manufacturer->SetSelection (0);

	_load_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::select_certificate, this));
	_download_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::download_certificate, this));
	_manufacturer->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ScreenDialog::setup_sensitivity, this));

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
		_certificate_text->SetValue (_certificate->certificate ());
	} catch (dcp::MiscError& e) {
		error_dialog (this, wxString::Format (_("Could not read certificate file (%s)"), e.what()));
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
	if (_manufacturer->GetStringSelection() == _("Doremi")) {
		DownloadCertificateDialog* d = new DoremiCertificateDialog (this, boost::bind (&ScreenDialog::load_certificate, this, _1));
		d->ShowModal ();
		d->Destroy ();
	} else if (_manufacturer->GetStringSelection() == _("Dolby")) {
		DownloadCertificateDialog* d = new DolbyCertificateDialog (this, boost::bind (&ScreenDialog::load_certificate, this, _1));
		d->ShowModal ();
		d->Destroy ();
	}

	setup_sensitivity ();
}

void
ScreenDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (_certificate);
	}

	_download_certificate->Enable (
		_manufacturer->GetStringSelection() == _("Doremi") ||
		_manufacturer->GetStringSelection() == _("Dolby")
		);
}
