/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <curl/curl.h>
#include <zip.h>
#include "lib/compose.hpp"
#include "lib/util.h"
#include "lib/internet.h"
#include "doremi_certificate_dialog.h"
#include "wx_util.h"

using std::string;
using boost::function;
using boost::optional;

DoremiCertificateDialog::DoremiCertificateDialog (wxWindow* parent, function<void (boost::filesystem::path)> load)
	: DownloadCertificateDialog (parent, load)
{
	add (_("Server serial number"), true);
	_serial = add (new wxTextCtrl (this, wxID_ANY));

	add_common_widgets ();
}

void
DoremiCertificateDialog::download ()
{
	string const serial = wx_to_std (_serial->GetValue ());
	if (serial.length() != 6) {
		error_dialog (this, _("Doremi serial numbers must have 6 digits"));
		return;
	}

	_message->SetLabel (_("Downloading certificate"));

	optional<string> error = get_from_zip_url (
		String::compose (
			"ftp://service:t3chn1c1an@ftp.doremilabs.com/Certificates/%1xxx/dcp2000-%2.dcicerts.zip",
			serial.substr(0, 3), serial
			),
		String::compose ("dcp2000-%1.cert.sha256.pem", serial),
		_load
		);

	if (error) {
		error_dialog (this, std_to_wx (error.get ()));
	} else {
		_message->SetLabel (wxT (""));
	}
}
