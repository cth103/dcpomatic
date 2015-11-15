/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "doremi_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/util.h"
#include "lib/signal_manager.h"
#include "lib/internet.h"
#include <curl/curl.h>
#include <zip.h>
#include <iostream>

using std::string;
using std::cout;
using boost::function;
using boost::optional;

DoremiCertificatePanel::DoremiCertificatePanel (wxWindow* parent, DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (parent, dialog)
{
	add_label_to_sizer (_table, this, _("Server serial number"), true);
	_serial = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (300, -1));
	_table->Add (_serial, 1, wxEXPAND);

	_serial->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&DownloadCertificateDialog::setup_sensitivity, _dialog));

	layout ();
}

void
DoremiCertificatePanel::download (wxStaticText* message)
{
	string const serial = wx_to_std (_serial->GetValue ());
	if (serial.length() != 6) {
		error_dialog (this, _("Doremi serial numbers must have 6 digits"));
		return;
	}

	message->SetLabel (_("Downloading certificate"));

	/* Hack: without this the SetLabel() above has no visible effect */
	wxMilliSleep (200);

	signal_manager->when_idle (boost::bind (&DoremiCertificatePanel::finish_download, this, serial, message));
}

void
DoremiCertificatePanel::finish_download (string serial, wxStaticText* message)
{
	/* Try dcp2000, imb and ims prefixes (see mantis #375) */

	optional<string> error = get_from_zip_url (
		String::compose (
			"ftp://service:t3chn1c1an@ftp.doremilabs.com/Certificates/%1xxx/dcp2000-%2.dcicerts.zip",
			serial.substr(0, 3), serial
			),
		String::compose ("dcp2000-%1.cert.sha256.pem", serial),
		boost::bind (&DownloadCertificatePanel::load, this, _1)
		);

	if (error) {
		error = get_from_zip_url (
		String::compose (
			"ftp://service:t3chn1c1an@ftp.doremilabs.com/Certificates/%1xxx/imb-%2.dcicerts.zip",
			serial.substr(0, 3), serial
			),
		String::compose ("imb-%1.cert.sha256.pem", serial),
		boost::bind (&DownloadCertificatePanel::load, this, _1)
		);
	}

	if (error) {
		error = get_from_zip_url (
		String::compose (
			"ftp://service:t3chn1c1an@ftp.doremilabs.com/Certificates/%1xxx/ims-%2.dcicerts.zip",
			serial.substr(0, 3), serial
			),
		String::compose ("ims-%1.cert.sha256.pem", serial),
		boost::bind (&DownloadCertificatePanel::load, this, _1)
		);
	}

	if (error) {
		message->SetLabel (wxT (""));
		error_dialog (this, std_to_wx (error.get ()));
	} else {
		message->SetLabel (_("Certificate downloaded"));
	}
}

bool
DoremiCertificatePanel::ready_to_download () const
{
	return !_serial->IsEmpty ();
}
