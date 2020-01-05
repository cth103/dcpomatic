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
#include <boost/function.hpp>

class PasswordEntry;

class CredentialsDownloadCertificatePanel : public DownloadCertificatePanel
{
public:
	CredentialsDownloadCertificatePanel (
			DownloadCertificateDialog* dialog,
			boost::function<boost::optional<std::string> ()> get_username,
			boost::function<void (std::string)> set_username,
			boost::function<void ()> unset_username,
			boost::function<boost::optional<std::string> ()> get_password,
			boost::function<void (std::string)> set_password,
			boost::function<void ()> unset_password
			);

	virtual bool ready_to_download () const;

private:
	void username_changed ();
	void password_changed ();

	boost::function<boost::optional<std::string> (void)> _get_username;
	boost::function<void (std::string)> _set_username;
	boost::function<void ()> _unset_username;
	boost::function<boost::optional<std::string> (void)> _get_password;
	boost::function<void (std::string)> _set_password;
	boost::function<void ()> _unset_password;

	wxTextCtrl* _username;
	PasswordEntry* _password;
};

#endif
