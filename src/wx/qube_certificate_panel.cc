/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "qube_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/internet.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include <boost/algorithm/string/predicate.hpp>

using std::string;
using std::list;
using boost::optional;

static string const base = "ftp://certificates.qubecinema.com/";

QubeCertificatePanel::QubeCertificatePanel (DownloadCertificateDialog* dialog, string type)
	: DownloadCertificatePanel (dialog)
	, _type (type)
{

}

void
QubeCertificatePanel::do_download ()
{
	list<string> files = ls_url(String::compose("%1SMPTE-%2/", base, _type));
	if (files.empty()) {
		error_dialog (this, _("Could not read certificates from Qube server."));
		return;
	}

	string const serial = wx_to_std(_serial->GetValue());
	optional<string> name;
	BOOST_FOREACH (string i, files) {
		if (boost::algorithm::starts_with(i, String::compose("%1-%2-", _type, serial))) {
			name = i;
			break;
		}
	}

	if (!name) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, wxString::Format(_("Could not find serial number %s"), std_to_wx(serial).data()));
		return;
	}

	optional<string> error = get_from_url (String::compose("%1SMPTE-%2/%3", base, _type, *name), true, boost::bind (&DownloadCertificatePanel::load, this, _1));

	if (error) {
		_dialog->message()->SetLabel(wxT(""));
		error_dialog (this, std_to_wx(*error));
	} else {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	}
}

wxString
QubeCertificatePanel::name () const
{
	return _("Qube") + " " + std_to_wx(_type);
}
