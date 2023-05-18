/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "recipient_dialog.h"
#include "static_text.h"
#include "table_dialog.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/util.h"
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/validate.h>
LIBDCP_ENABLE_WARNINGS
#include <iostream>


using std::cout;
using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static string
column (string s)
{
	return s;
}


RecipientDialog::RecipientDialog (
	wxWindow* parent, wxString title, string name, string notes, vector<string> emails, int utc_offset_hour, int utc_offset_minute, optional<dcp::Certificate> recipient
	)
	: wxDialog (parent, wxID_ANY, title)
	, _recipient (recipient)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
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

	add_label_to_sizer (_sizer, this, _("UTC offset (time zone)"), true, wxGBPosition (r, 0));
	_utc_offset = new wxChoice (this, wxID_ANY);
	_sizer->Add (_utc_offset, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_sizer, this, _("Email addresses for KDM delivery"), false, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	copy (emails.begin(), emails.end(), back_inserter (_emails));

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Address")));
	_email_list = new EditableList<string, EmailDialog> (
		this, columns, bind(&RecipientDialog::get_emails, this), bind(&RecipientDialog::set_emails, this, _1), bind(&column, _1),
		EditableListTitle::VISIBLE,
		EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);

	_sizer->Add (_email_list, wxGBPosition (r, 0), wxGBSpan (1, 2), wxEXPAND);
	++r;

        wxClientDC dc (this);
	auto font = _name->GetFont ();
	font.SetFamily (wxFONTFAMILY_TELETYPE);
	dc.SetFont (font);
	auto size = dc.GetTextExtent(wxT("1234567890123456789012345678"));
	size.SetHeight (-1);

	add_label_to_sizer (_sizer, this, _("Recipient certificate"), true, wxGBPosition (r, 0));
	auto s = new wxBoxSizer (wxHORIZONTAL);
	_recipient_thumbprint = new StaticText (this, wxT (""), wxDefaultPosition, size);
	_recipient_thumbprint->SetFont (font);
	set_recipient (recipient);
	_get_recipient_from_file = new Button (this, _("Get from file..."));
	s->Add (_recipient_thumbprint, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	s->Add (_get_recipient_from_file, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_X_GAP);
	_sizer->Add (s, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_sizer, this, _("Other trusted devices"), true, wxGBPosition (r, 0));
	++r;

	_name->Bind (wxEVT_TEXT, boost::bind (&RecipientDialog::setup_sensitivity, this));
	_get_recipient_from_file->Bind (wxEVT_BUTTON, boost::bind (&RecipientDialog::get_recipient_from_file, this));

	overall_sizer->Add (_sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	/* Default to UTC */
	size_t sel = get_offsets (_offsets);
	for (size_t i = 0; i < _offsets.size(); ++i) {
		_utc_offset->Append (_offsets[i].name);
		if (_offsets[i].hour == utc_offset_hour && _offsets[i].minute == utc_offset_minute) {
			sel = i;
		}
	}

	_utc_offset->SetSelection (sel);

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	setup_sensitivity ();
}


string
RecipientDialog::name () const
{
	return wx_to_std (_name->GetValue());
}


string
RecipientDialog::notes () const
{
	return wx_to_std (_notes->GetValue());
}


optional<dcp::Certificate>
RecipientDialog::recipient () const
{
	return _recipient;
}


void
RecipientDialog::load_recipient (boost::filesystem::path file)
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
RecipientDialog::get_recipient_from_file ()
{
	auto d = make_wx<wxFileDialog>(this, _("Select Certificate File"));
	if (d->ShowModal () == wxID_OK) {
		load_recipient (boost::filesystem::path (wx_to_std (d->GetPath ())));
	}

	setup_sensitivity ();
}


void
RecipientDialog::setup_sensitivity ()
{
	auto ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (static_cast<bool>(_recipient) && !_name->GetValue().IsEmpty());
	}
}


void
RecipientDialog::set_recipient (optional<dcp::Certificate> r)
{
	_recipient = r;

	if (_recipient) {
		_recipient_thumbprint->SetLabel (std_to_wx (_recipient->thumbprint ()));
		_sizer->Layout ();
	}
}


vector<string>
RecipientDialog::get_emails () const
{
	return _emails;
}


void
RecipientDialog::set_emails (vector<string> e)
{
	_emails = e;
}


vector<string>
RecipientDialog::emails () const
{
	return _emails;
}


int
RecipientDialog::utc_offset_hour () const
{
	int const sel = _utc_offset->GetSelection();
	if (sel < 0 || sel > int (_offsets.size())) {
		return 0;
	}

	return _offsets[sel].hour;
}

int
RecipientDialog::utc_offset_minute () const
{
	int const sel = _utc_offset->GetSelection();
	if (sel < 0 || sel > int (_offsets.size())) {
		return 0;
	}

	return _offsets[sel].minute;
}


