/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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
#include "qube_certificate_panel.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/internet.h"
#include <boost/algorithm/string.hpp>


using std::string;
using std::list;
using namespace boost::algorithm;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static string const base = "ftp://certificates.qubecinema.com/";


QubeCertificatePanel::QubeCertificatePanel (DownloadCertificateDialog* dialog, string type)
	: DownloadCertificatePanel (dialog)
	, _type (type)
{

}


void
QubeCertificatePanel::do_download ()
{
	auto files = ls_url(fmt::format("{}SMPTE-{}/", base, _type));
	if (files.empty()) {
		error_dialog (this, _("Could not read certificates from Qube server."));
		return;
	}

	auto serial = wx_to_std(_serial->GetValue());
	trim(serial);

	optional<string> name;
	for (auto i: files) {
		if (boost::algorithm::starts_with(i, fmt::format("{}-{}-", _type, serial))) {
			name = i;
			break;
		}
	}

	if (!name) {
		_dialog->message()->SetLabel({});
		error_dialog (this, wxString::Format(_("Could not find serial number %s"), std_to_wx(serial).data()));
		return;
	}

	auto error = get_from_url (fmt::format("{}SMPTE-{}/{}", base, _type, *name), true, false, boost::bind(&DownloadCertificatePanel::load_certificate, this, _1, _2));

	if (error) {
		_dialog->message()->SetLabel({});
		error_dialog (this, std_to_wx(*error));
	} else {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	}
}


wxString
QubeCertificatePanel::name () const
{
	return wxString::Format(_("Qube %s"), std_to_wx(_type));
}
