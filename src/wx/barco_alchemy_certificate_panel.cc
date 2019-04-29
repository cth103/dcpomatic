/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "barco_alchemy_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/internet.h"
#include "lib/compose.hpp"
#include "lib/config.h"

using std::string;
using boost::optional;

BarcoAlchemyCertificatePanel::BarcoAlchemyCertificatePanel (DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (dialog)
{

}

bool
BarcoAlchemyCertificatePanel::ready_to_download () const
{
	return _serial->GetValue().Length() == 10;
}

void
BarcoAlchemyCertificatePanel::do_download ()
{
	Config* config = Config::instance ();
	if (!config->barco_username() || !config->barco_password()) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, _("No Barco username/password configured.  Add your account details to the Accounts page in Preferences."));
		return;
	}

	string const serial = wx_to_std (_serial->GetValue());
	string const url = String::compose (
		"ftp://%1:%2@certificates.barco.com/%3xxx/%4/Barco-ICMP.%5_cert.pem",
		Config::instance()->barco_username().get(),
		Config::instance()->barco_password().get(),
		serial.substr(0, 7),
		serial,
		serial
		);

	optional<string> error = get_from_url (url, true, false, boost::bind (&DownloadCertificatePanel::load, this, _1));
	if (error) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, std_to_wx(*error));
	} else {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	}
}

wxString
BarcoAlchemyCertificatePanel::name () const
{
	return _("Barco Alchemy");
}
