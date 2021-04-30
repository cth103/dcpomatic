/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "download_certificate_dialog.h"
#include "credentials_download_certificate_panel.h"
#include "password_entry.h"
#include "wx_util.h"

using std::string;
using std::function;
using boost::optional;

CredentialsDownloadCertificatePanel::CredentialsDownloadCertificatePanel (
		DownloadCertificateDialog* dialog,
		function<optional<string> ()> get_username,
		function<void (string)> set_username,
		function<void ()> unset_username,
		function<optional<string> ()> get_password,
		function<void (string)> set_password,
		function<void ()> unset_password
		)
	: DownloadCertificatePanel (dialog)
	, _get_username (get_username)
	, _set_username (set_username)
	, _unset_username (unset_username)
	, _get_password (get_password)
	, _set_password (set_password)
	, _unset_password (unset_password)
{
	add_label_to_sizer (_table, this, _("User name"), true, 0, wxALIGN_CENTER_VERTICAL);
	_username = new wxTextCtrl (this, wxID_ANY, std_to_wx(_get_username().get_value_or("")), wxDefaultPosition, wxSize(300, -1));
	_table->Add (_username, 1, wxEXPAND);

	add_label_to_sizer (_table, this, _("Password"), true, 0, wxALIGN_CENTER_VERTICAL);
	_password = new PasswordEntry (this);
	_password->set (_get_password().get_value_or(""));
	_table->Add (_password->get_panel(), 1, wxEXPAND);

	_username->Bind (wxEVT_TEXT, boost::bind(&CredentialsDownloadCertificatePanel::username_changed, this));
	_password->Changed.connect (boost::bind(&CredentialsDownloadCertificatePanel::password_changed, this));

	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);
}

bool
CredentialsDownloadCertificatePanel::ready_to_download () const
{
	return DownloadCertificatePanel::ready_to_download() && static_cast<bool>(_get_username()) && _get_username().get() != "" && static_cast<bool>(_get_password()) && _get_password().get() != "";
}

void
CredentialsDownloadCertificatePanel::username_changed ()
{
	wxString const s = _username->GetValue();
	if (!s.IsEmpty()) {
		_set_username (wx_to_std(s));
	} else {
		_unset_username ();
	}
	_dialog->setup_sensitivity ();
}

void
CredentialsDownloadCertificatePanel::password_changed ()
{
	string const s = _password->get();
	if (!s.empty()) {
		_set_password (s);
	} else {
		_unset_password ();
	}
	_dialog->setup_sensitivity ();
}

