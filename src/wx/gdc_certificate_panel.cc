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

#include "gdc_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/internet.h"
#include "lib/compose.hpp"
#include "lib/config.h"

using std::string;
using boost::optional;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

GDCCertificatePanel::GDCCertificatePanel (DownloadCertificateDialog* dialog)
	: CredentialsDownloadCertificatePanel (
			dialog,
			bind(&Config::gdc_username, Config::instance()),
			bind(&Config::set_gdc_username, Config::instance(), _1),
			bind(&Config::unset_gdc_username, Config::instance()),
			bind(&Config::gdc_password, Config::instance()),
			bind(&Config::set_gdc_password, Config::instance(), _1),
			bind(&Config::unset_gdc_password, Config::instance())
			)
{

}

void
GDCCertificatePanel::do_download ()
{
	string const url = String::compose(
		"ftp://%1:%2@ftp.gdc-tech.com/SHA256/A%3.crt.pem",
		Config::instance()->gdc_username().get(),
		Config::instance()->gdc_password().get(),
		wx_to_std(_serial->GetValue())
		);

	optional<string> error = get_from_url (url, true, false, boost::bind(&DownloadCertificatePanel::load_certificate, this, _1));

	if (error) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, std_to_wx(*error));
	} else {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	}
}

wxString
GDCCertificatePanel::name () const
{
	return _("GDC");
}
