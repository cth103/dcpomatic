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
#include <boost/algorithm/string.hpp>


using std::string;
using namespace boost::algorithm;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


BarcoAlchemyCertificatePanel::BarcoAlchemyCertificatePanel (DownloadCertificateDialog* dialog)
	: CredentialsDownloadCertificatePanel (
			dialog,
			boost::bind(&Config::barco_username, Config::instance()),
			boost::bind(&Config::set_barco_username, Config::instance(), _1),
			boost::bind(&Config::unset_barco_username, Config::instance()),
			boost::bind(&Config::barco_password, Config::instance()),
			boost::bind(&Config::set_barco_password, Config::instance(), _1),
			boost::bind(&Config::unset_barco_password, Config::instance())
			)
{

}

bool
BarcoAlchemyCertificatePanel::ready_to_download () const
{
	return CredentialsDownloadCertificatePanel::ready_to_download() && _serial->GetValue().Length() == 10;
}

void
BarcoAlchemyCertificatePanel::do_download ()
{
	string serial = wx_to_std (_serial->GetValue());
	trim(serial);
	string url = String::compose (
		"ftp://%1:%2@certificates.barco.com/%3xxx/%4/Barco-ICMP.%5_cert.pem",
		Config::instance()->barco_username().get(),
		Config::instance()->barco_password().get(),
		serial.substr(0, 7),
		serial,
		serial
		);
	trim(url);

	auto error = get_from_url (url, true, false, boost::bind (&DownloadCertificatePanel::load_certificate, this, _1, _2));
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
