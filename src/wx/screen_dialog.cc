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
using std::vector;
using boost::optional;
using boost::bind;

class FileDialogWrapper
{
public:
	FileDialogWrapper (wxWindow* parent)
		: _parent (parent)
	{
		_dialog = new wxFileDialog (parent, _("Select certificate file"));
	}

	void set (dcp::Certificate) {}

	dcp::Certificate get () {
		return dcp::Certificate (dcp::file_to_string (wx_to_std (_dialog->GetPath ())));
	}

	int ShowModal ()
	{
		return _dialog->ShowModal ();
	}

	void Destroy ()
	{
		_dialog->Destroy ();
		/* eek! */
		delete this;
	}

private:
	wxWindow* _parent;
	wxFileDialog* _dialog;
};

static string
column (dcp::Certificate c)
{
	return c.thumbprint ();
}

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, optional<dcp::Certificate> recipient, vector<dcp::Certificate> trusted_devices)
	: wxDialog (parent, wxID_ANY, std_to_wx (title))
	, _recipient (recipient)
	, _trusted_devices (trusted_devices)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	_sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	int r = 0;

	add_label_to_sizer (_sizer, this, _("Name"), true, wxGBPosition (r, 0));
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1));
	_sizer->Add (_name, wxGBPosition (r, 1));
	++r;

        wxClientDC dc (this);
	wxFont font = _name->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	dc.SetFont (font);
        wxSize size = dc.GetTextExtent (wxT ("1234567890123456789012345678"));
        size.SetHeight (-1);

	add_label_to_sizer (_sizer, this, _("Recipient certificate"), true, wxGBPosition (r, 0));
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_recipient_thumbprint = new wxStaticText (this, wxID_ANY, wxT (""), wxDefaultPosition, size);
	_recipient_thumbprint->SetFont (font);
	set_recipient (recipient);
	_get_recipient_from_file = new wxButton (this, wxID_ANY, _("Get from file..."));
	_download_recipient = new wxButton (this, wxID_ANY, _("Download..."));
	s->Add (_recipient_thumbprint, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	s->Add (_get_recipient_from_file, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	s->Add (_download_recipient, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	_sizer->Add (s, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_sizer, this, _("Other trusted devices"), true, wxGBPosition (r, 0));
	++r;

	vector<string> columns;
	columns.push_back (wx_to_std (_("Thumbprint")));
	_trusted_device_list = new EditableList<dcp::Certificate, FileDialogWrapper> (
		this, columns, bind (&ScreenDialog::trusted_devices, this), bind (&ScreenDialog::set_trusted_devices, this, _1), bind (&column, _1), false
		);

	_sizer->Add (_trusted_device_list, wxGBPosition (r, 0), wxGBSpan (1, 3), wxEXPAND);
	++r;

	_name->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ScreenDialog::setup_sensitivity, this));
	_get_recipient_from_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::get_recipient_from_file, this));
	_download_recipient->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::download_recipient, this));

	overall_sizer->Add (_sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	setup_sensitivity ();
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
		set_recipient (dcp::Certificate (dcp::file_to_string (file)));
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
		set_recipient (d->certificate ());
	}
	d->Destroy ();
	setup_sensitivity ();
}

void
ScreenDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(_recipient) && !_name->GetValue().IsEmpty());
	}
}

void
ScreenDialog::set_recipient (optional<dcp::Certificate> r)
{
	_recipient = r;

	if (_recipient) {
		_recipient_thumbprint->SetLabel (std_to_wx (_recipient->thumbprint ()));
		_sizer->Layout ();
	}
}
