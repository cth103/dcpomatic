/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
#include "download_certificate_panel.h"
#include "wx_util.h"
#include "lib/signal_manager.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/util.h>
#include <boost/bind/bind.hpp>


using std::string;
using boost::optional;


DownloadCertificatePanel::DownloadCertificatePanel (DownloadCertificateDialog* dialog)
	: wxPanel (dialog->notebook(), wxID_ANY)
	, _dialog (dialog)
{
	_overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_overall_sizer);

	_table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_table->AddGrowableCol (1, 1);

	_overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	add_label_to_sizer (_table, this, _("Serial number"), true, 0, wxALIGN_CENTER_VERTICAL);
	_serial = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxSize (300, -1));
	_table->Add (_serial, 1, wxEXPAND);

	_serial->Bind (wxEVT_TEXT, boost::bind (&DownloadCertificateDialog::setup_sensitivity, _dialog));

	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);
}


optional<string>
DownloadCertificatePanel::load_certificate (boost::filesystem::path file, string url)
{
	try {
		_certificate = dcp::Certificate (dcp::file_to_string(file));
		_url = url;
	} catch (dcp::MiscError& e) {
		return fmt::format(wx_to_std(_("Could not read certificate file ({})")), e.what());
	}
	return {};
}


optional<string>
DownloadCertificatePanel::load_certificate_from_chain (boost::filesystem::path file, string url)
{
	try {
		_certificate = dcp::CertificateChain(dcp::file_to_string(file)).leaf();
		_url = url;
	} catch (dcp::MiscError& e) {
		return fmt::format(wx_to_std(_("Could not read certificate file ({})")), e.what());
	}
	return {};
}


optional<dcp::Certificate>
DownloadCertificatePanel::certificate () const
{
	return _certificate;

}


optional<string>
DownloadCertificatePanel::url () const
{
	return _url;

}

void
DownloadCertificatePanel::download ()
{
	_dialog->message()->SetLabel (_("Downloading certificate"));

	/* Hack: without this the SetLabel() above has no visible effect */
	wxMilliSleep (200);
	wxYield();

	signal_manager->when_idle (boost::bind(&DownloadCertificatePanel::do_download, this));
}


bool
DownloadCertificatePanel::ready_to_download () const
{
	return !_serial->IsEmpty();
}
