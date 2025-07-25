/*
    Copyright (C) 2012-2022 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_button.h"
#include "download_certificate_dialog.h"
#include "file_dialog.h"
#include "screen_dialog.h"
#include "static_text.h"
#include "table_dialog.h"
#include "wx_util.h"
#include "lib/util.h"
#include <dcp/warnings.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/validate.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


class TrustedDeviceDialog : public TableDialog
{
public:
	explicit TrustedDeviceDialog (wxWindow* parent)
		: TableDialog (parent, _("Trusted Device"), 3, 1, true)
	{
		add (_("Thumbprint"), true);
		_thumbprint = add(new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxSize(300, -1), wxTE_READONLY));
		_file = add (new Button(this, _("Load certificate...")));

		layout ();

		_file->Bind (wxEVT_BUTTON, bind(&TrustedDeviceDialog::load_certificate, this));

		setup_sensitivity();
	}

	void load_certificate ()
	{
		FileDialog dialog(this, _("Trusted Device certificate"), wxEmptyString, wxFD_DEFAULT_STYLE, "SelectCertificatePath");
		if (!dialog.show()) {
			return;
		}

		try {
			_certificate = dcp::CertificateChain(dcp::file_to_string(dialog.paths()[0])).leaf();
			_thumbprint->SetValue (std_to_wx(_certificate->thumbprint()));
			setup_sensitivity();
		} catch (dcp::MiscError& e) {
			error_dialog(this, wxString::Format(_("Could not load certificate (%s)"), std_to_wx(e.what())));
		}
	}

	void set (TrustedDevice t)
	{
		_certificate = t.certificate ();
		_thumbprint->SetValue (std_to_wx(t.thumbprint()));
		setup_sensitivity();
	}

	vector<TrustedDevice> get()
	{
		auto const t = wx_to_std (_thumbprint->GetValue());
		if (_certificate && _certificate->thumbprint() == t) {
			return { TrustedDevice(*_certificate) };
		} else if (t.length() == 28) {
			return { TrustedDevice(t) };
		}

		return {};
	}

private:
	void setup_sensitivity()
	{
		auto ok = dynamic_cast<wxButton*>(FindWindowById(wxID_OK, this));
		DCPOMATIC_ASSERT(ok);
		ok->Enable(static_cast<bool>(_certificate));
	}

	wxTextCtrl* _thumbprint;
	wxButton* _file;
	boost::optional<dcp::Certificate> _certificate;
};


ScreenDialog::ScreenDialog (
	wxWindow* parent,
	wxString title,
	string name,
	string notes,
	optional<dcp::Certificate> recipient,
	optional<string> recipient_file,
	vector<TrustedDevice> trusted_devices
	)
	: wxDialog (parent, wxID_ANY, title)
	, _recipient (recipient)
	, _trusted_devices (trusted_devices)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	_sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	int r = 0;

	wxFont subheading_font(*wxNORMAL_FONT);
	subheading_font.SetWeight(wxFONTWEIGHT_BOLD);

	auto subheading = new StaticText(this, _("Details"));
	subheading->SetFont(subheading_font);
	_sizer->Add(subheading, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	add_label_to_sizer(_sizer, this, _("Name"), true, wxGBPosition(r, 0), wxDefaultSpan, true);
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1));
	_sizer->Add (_name, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer(_sizer, this, _("Notes"), true, wxGBPosition(r, 0), wxDefaultSpan, true);
	_notes = new wxTextCtrl (this, wxID_ANY, std_to_wx(notes), wxDefaultPosition, wxSize(320, -1));
	_sizer->Add (_notes, wxGBPosition(r, 1));
	++r;

	subheading = new StaticText(this, _("Recipient"));
	subheading->SetFont(subheading_font);
	_sizer->Add(subheading, wxGBPosition(r, 0), wxGBSpan(1, 2), wxTOP, DCPOMATIC_SUBHEADING_TOP_PAD);
	++r;

	_get_recipient_from_file = new Button (this, _("Get from file..."));
	_download_recipient = new Button (this, _("Download..."));
	auto s = new wxBoxSizer (wxHORIZONTAL);
	s->Add (_get_recipient_from_file, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	s->Add (_download_recipient, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	_sizer->Add(s, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	auto add_certificate_detail = [&r, this](wxString name, wxStaticText** value, wxSize size = wxDefaultSize) {
		add_label_to_sizer(_sizer, this, name, true, wxGBPosition(r, 0), wxDefaultSpan, true);
		*value = new StaticText(this, {}, wxDefaultPosition, size);
		_sizer->Add(*value, wxGBPosition(r, 1));
		++r;
	};

        wxClientDC dc (this);
	wxFont teletype_font = _name->GetFont();
	teletype_font.SetFamily(wxFONTFAMILY_TELETYPE);
	dc.SetFont(teletype_font);
        wxSize size = dc.GetTextExtent(char_to_wx("1234567890123456789012345678"));
        size.SetHeight (-1);

	add_certificate_detail(_("Thumbprint"), &_recipient_thumbprint, size);
	_recipient_thumbprint->SetFont(teletype_font);

	add_label_to_sizer(_sizer, this, _("Filename"), true, wxGBPosition(r, 0), wxDefaultSpan, true);
	_recipient_file = new wxStaticText(this, wxID_ANY, {}, wxDefaultPosition, wxSize(600, -1), wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);
	set_recipient_file(recipient_file.get_value_or(""));
	_sizer->Add (_recipient_file, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_Y_GAP);
	++r;

	add_certificate_detail(_("Subject common name"), &_subject_common_name);
	add_certificate_detail(_("Subject organization name"), &_subject_organization_name);
	add_certificate_detail(_("Issuer common name"), &_issuer_common_name);
	add_certificate_detail(_("Issuer organization name"), &_issuer_organization_name);
	add_certificate_detail(_("Not valid before"), &_not_valid_before);
	add_certificate_detail(_("Not valid after"), &_not_valid_after);

	set_recipient (recipient);

	{
		int flags = wxALIGN_CENTER_VERTICAL | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		auto m = new StaticText (this, _("Other trusted devices") + char_to_wx(":"));
#else
		auto m = new StaticText (this, _("Other trusted devices"));
#endif
		m->SetFont(subheading_font);
		_sizer->Add(m, wxGBPosition(r, 0), wxDefaultSpan, flags, DCPOMATIC_SUBHEADING_TOP_PAD);
	}
	++r;

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Thumbprint")));
	_trusted_device_list = new EditableList<TrustedDevice, TrustedDeviceDialog> (
		this,
		columns,
		bind (&ScreenDialog::trusted_devices, this),
		bind (&ScreenDialog::set_trusted_devices, this, _1),
		[] (TrustedDevice const& d, int) {
			return d.thumbprint();
		},
		EditableListTitle::INVISIBLE,
		EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);

	_sizer->Add(_trusted_device_list, wxGBPosition (r, 0), wxGBSpan (1, 3), wxEXPAND | wxLEFT, DCPOMATIC_SIZER_X_GAP);
	++r;

	_name->Bind (wxEVT_TEXT, boost::bind (&ScreenDialog::setup_sensitivity, this));
	_get_recipient_from_file->Bind (wxEVT_BUTTON, boost::bind (&ScreenDialog::get_recipient_from_file, this));
	_download_recipient->Bind (wxEVT_BUTTON, boost::bind (&ScreenDialog::download_recipient, this));

	overall_sizer->Add (_sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
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


optional<string>
ScreenDialog::recipient_file () const
{
	auto const f = wx_to_std(_recipient_file->GetLabel());
	if (f.empty()) {
		return {};
	}
	return f;
}


void
ScreenDialog::load_recipient (boost::filesystem::path file)
{
	try {
		/* Load this as a chain, in case it is one, and then pick the leaf certificate */
		dcp::CertificateChain c (dcp::file_to_string(file));
		if (c.unordered().empty()) {
			error_dialog (this, _("Could not read certificate file."));
			return;
		}
		set_recipient (c.leaf ());
		set_recipient_file(file.string());
	} catch (dcp::MiscError& e) {
		error_dialog (this, _("Could not read certificate file."), std_to_wx(e.what()));
	}
}


void
ScreenDialog::get_recipient_from_file ()
{
	FileDialog dialog(this, _("Select Certificate File"), wxEmptyString, wxFD_DEFAULT_STYLE , "SelectCertificatePath");
	if (dialog.show()) {
		load_recipient(dialog.paths()[0]);
	}

	setup_sensitivity ();
}


void
ScreenDialog::download_recipient ()
{
	DownloadCertificateDialog dialog(this);
	if (dialog.ShowModal() == wxID_OK) {
		set_recipient(dialog.certificate());
		set_recipient_file(dialog.url());
	}
	setup_sensitivity ();
}


void
ScreenDialog::setup_sensitivity ()
{
	auto ok = dynamic_cast<wxButton*> (FindWindowById(wxID_OK, this));
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
		_subject_common_name->SetLabel(std_to_wx(_recipient->subject_common_name()));
		_subject_organization_name->SetLabel(std_to_wx(_recipient->subject_organization_name()));
		_issuer_common_name->SetLabel(std_to_wx(_recipient->issuer_common_name()));
		_issuer_organization_name->SetLabel(std_to_wx(_recipient->issuer_organization_name()));
		_not_valid_before->SetLabel(std_to_wx(_recipient->not_before().as_string()));
		_not_valid_after->SetLabel(std_to_wx(_recipient->not_after().as_string()));
		_sizer->Layout ();
	}
}


void
ScreenDialog::set_recipient_file(string file)
{
	checked_set(_recipient_file, file);
	_recipient_file->SetToolTip(std_to_wx(file));
}

