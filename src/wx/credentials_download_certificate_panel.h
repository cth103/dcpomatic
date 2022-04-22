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


#ifndef CREDENTIALS_DOWNLOAD_CERTIFICATE_PANEL_H
#define CREDENTIALS_DOWNLOAD_CERTIFICATE_PANEL_H


#include "download_certificate_panel.h"


class PasswordEntry;


class CredentialsDownloadCertificatePanel : public DownloadCertificatePanel
{
public:
	CredentialsDownloadCertificatePanel (
			DownloadCertificateDialog* dialog,
			std::function<boost::optional<std::string> ()> get_username,
			std::function<void (std::string)> set_username,
			std::function<void ()> unset_username,
			std::function<boost::optional<std::string> ()> get_password,
			std::function<void (std::string)> set_password,
			std::function<void ()> unset_password
			);

	bool ready_to_download () const override;

private:
	void username_changed ();
	void password_changed ();

	std::function<boost::optional<std::string> (void)> _get_username;
	std::function<void (std::string)> _set_username;
	std::function<void ()> _unset_username;
	std::function<boost::optional<std::string> (void)> _get_password;
	std::function<void (std::string)> _set_password;
	std::function<void ()> _unset_password;

	wxTextCtrl* _username;
	PasswordEntry* _password;
};

#endif
