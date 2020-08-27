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

#include "christie_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/internet.h"
#include "lib/compose.hpp"
#include "lib/config.h"

using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

ChristieCertificatePanel::ChristieCertificatePanel (DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (dialog)
{

}

void
ChristieCertificatePanel::do_download ()
{
	Config* config = Config::instance ();
	if (!config->christie_username() || !config->christie_password()) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, _("No Christie username/password configured.  Add your account details to the Accounts page in Preferences."));
		return;
	}

	string const prefix = String::compose(
		"ftp://%1:%2@certificates.christiedigital.com/Certificates/",
		Config::instance()->christie_username().get(),
		Config::instance()->christie_password().get()
		);

	string serial = wx_to_std (_serial->GetValue());
	serial.insert (0, 12 - serial.length(), '0');

	string const url = String::compose ("%1F-IMB/F-IMB_%2_sha256.pem", prefix, serial);

	optional<string> all_errors;

	optional<string> error = get_from_url (url, true, false, boost::bind(&DownloadCertificatePanel::load_certificate_from_chain, this, _1));
	if (error) {
		all_errors = *error;

		string const url = String::compose ("%1IMB-S2/IMB-S2_%2_sha256.pem", prefix, serial);

		error = get_from_url (url, true, false, boost::bind(&DownloadCertificatePanel::load_certificate_from_chain, this, _1));
		if (error) {
			*all_errors += "\n" + *error;
		}
	}

	if (all_errors) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, std_to_wx(*all_errors));
	} else {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	}
}

wxString
ChristieCertificatePanel::name () const
{
	return _("Christie");
}
