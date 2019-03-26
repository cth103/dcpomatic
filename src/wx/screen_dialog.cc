/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "screen_dialog.h"
#include "wx_util.h"
#include "file_dialog_wrapper.h"
#include "static_text.h"
#include "download_certificate_dialog.h"
#include "table_dialog.h"
#include "dcpomatic_button.h"
#include "lib/compose.hpp"
#include "lib/util.h"
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/filepicker.h>
#include <wx/validate.h>
#include <iostream>

using std::string;
using std::cout;
using std::vector;
using boost::optional;
using boost::bind;

static string
column (TrustedDevice d)
{
	return d.thumbprint ();
}

class TrustedDeviceDialog : public TableDialog
{
public:
	explicit TrustedDeviceDialog (wxWindow* parent)
		: TableDialog (parent, _("Trusted Device"), 3, 1, true)
	{
		add (_("Thumbprint"), true);
		_thumbprint = add (new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(300, -1)));
		_file = add (new Button(this, _("Load certificate...")));

		layout ();

		_file->Bind (wxEVT_BUTTON, bind(&TrustedDeviceDialog::load_certificate, this));
	}

	void load_certificate ()
	{
		wxFileDialog* d = new wxFileDialog (this, _("Trusted Device certificate"));
		d->ShowModal ();
		try {
			_certificate = dcp::Certificate(dcp::file_to_string(wx_to_std(d->GetPath())));
			_thumbprint->SetValue (std_to_wx(_certificate->thumbprint()));
		} catch (dcp::MiscError& e) {
			error_dialog (this, wxString::Format(_("Could not load certficate (%s)"), std_to_wx(e.what())));
		}
	}

	void set (TrustedDevice t)
	{
		_certificate = t.certificate ();
		_thumbprint->SetValue (std_to_wx(t.thumbprint()));
	}

	optional<TrustedDevice> get ()
	{
		string const t = wx_to_std (_thumbprint->GetValue ());
		if (_certificate && _certificate->thumbprint() == t) {
			return TrustedDevice (*_certificate);
		} else if (t.length() == 28) {
			return TrustedDevice (t);
		}

		return optional<TrustedDevice> ();
	}

private:
	wxTextCtrl* _thumbprint;
	wxButton* _file;
	boost::optional<dcp::Certificate> _certificate;
};

ScreenDialog::ScreenDialog (
	wxWindow* parent, wxString title, string name, string notes, optional<dcp::Certificate> recipient, vector<TrustedDevice> trusted_devices
	)
	: wxDialog (parent, wxID_ANY, title)
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

	add_label_to_sizer (_sizer, this, _("Notes"), true, wxGBPosition (r, 0));
	_notes = new wxTextCtrl (this, wxID_ANY, std_to_wx (notes), wxDefaultPosition, wxSize (320, -1));
	_sizer->Add (_notes, wxGBPosition (r, 1));
	++r;

        wxClientDC dc (this);
	wxFont font = _name->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	dc.SetFont (font);
        wxSize size = dc.GetTextExtent (wxT ("1234567890123456789012345678"));
        size.SetHeight (-1);

	add_label_to_sizer (_sizer, this, _("Recipient certificate"), true, wxGBPosition (r, 0));
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_recipient_thumbprint = new StaticText (this, wxT (""), wxDefaultPosition, size);
	_recipient_thumbprint->SetFont (font);
	set_recipient (recipient);
	_get_recipient_from_file = new Button (this, _("Get from file..."));
	_download_recipient = new Button (this, _("Download..."));
	s->Add (_recipient_thumbprint, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	s->Add (_get_recipient_from_file, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	s->Add (_download_recipient, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	_sizer->Add (s, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_sizer, this, _("Other trusted devices"), true, wxGBPosition (r, 0));
	++r;

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(wx_to_std(_("Thumbprint"))));
	_trusted_device_list = new EditableList<TrustedDevice, TrustedDeviceDialog> (
		this,
		columns,
		bind (&ScreenDialog::trusted_devices, this),
		bind (&ScreenDialog::set_trusted_devices, this, _1),
		bind (&column, _1),
		false
		);

	_sizer->Add (_trusted_device_list, wxGBPosition (r, 0), wxGBSpan (1, 3), wxEXPAND);
	++r;

	_name->Bind (wxEVT_TEXT, boost::bind (&ScreenDialog::setup_sensitivity, this));
	_get_recipient_from_file->Bind (wxEVT_BUTTON, boost::bind (&ScreenDialog::get_recipient_from_file, this));
	_download_recipient->Bind (wxEVT_BUTTON, boost::bind (&ScreenDialog::download_recipient, this));

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

string
ScreenDialog::notes () const
{
	return wx_to_std (_notes->GetValue());
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
		/* Load this as a chain, in case it is one, and then pick the leaf certificate */
		dcp::CertificateChain c (dcp::file_to_string (file));
		if (c.unordered().empty()) {
			error_dialog (this, _("Could not read certificate file."));
			return;
		}
		set_recipient (c.leaf ());
	} catch (dcp::MiscError& e) {
		error_dialog (this, _("Could not read certificate file."), std_to_wx(e.what()));
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
