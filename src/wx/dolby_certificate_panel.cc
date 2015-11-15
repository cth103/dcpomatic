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

#include "dolby_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/internet.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include <curl/curl.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using std::vector;
using std::cout;
using boost::optional;
using boost::algorithm::split;
using boost::algorithm::is_any_of;

DolbyCertificatePanel::DolbyCertificatePanel (wxWindow* parent, DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (parent, dialog)
{
	add_label_to_sizer (_table, this, _("Country"), true);
	_country = new wxChoice (this, wxID_ANY);
	_table->Add (_country, 1, wxEXPAND);
	_country->Append (N_("Hashemite Kingdom of Jordan"));

	add_label_to_sizer (_table, this, _("Cinema"), true);
	_cinema = new wxChoice (this, wxID_ANY);
	_table->Add (_cinema, 1, wxEXPAND);
	_cinema->Append (N_("Motion Picture Solutions London Mobile & QC"));

	add_label_to_sizer (_table, this, _("Serial number"), true);
	_serial = new wxChoice (this, wxID_ANY);
	_table->Add (_serial, 1, wxEXPAND);

	layout ();

	_country->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificatePanel::country_selected, this));
	_cinema->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificatePanel::cinema_selected, this));
	_serial->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DownloadCertificateDialog::setup_sensitivity, _dialog));

	_country->Clear ();
	_cinema->Clear ();
}

list<string>
DolbyCertificatePanel::get_dir (string dir) const
{
	string url = String::compose ("ftp://dolbyrootcertificates:houro61l@ftp.dolby.co.uk/SHA256/%1", dir);
	return ftp_ls (url, false);
}

void
DolbyCertificatePanel::setup_countries ()
{
	if (_country->GetCount() > 0) {
		/* Already set up */
		return;
	}

	_country->Append (_("Fetching..."));
	_country->SetSelection (0);

	/* See DoremiCertificatePanel for discussion about this daft delay */
	wxMilliSleep (200);

	signal_manager->when_idle (boost::bind (&DolbyCertificatePanel::finish_setup_countries, this));
}

void
DolbyCertificatePanel::finish_setup_countries ()
{
	try {
		list<string> const c = get_dir ("");
		_country->Clear ();
		BOOST_FOREACH (string i, c) {
			_country->Append (std_to_wx (i));
		}
	} catch (NetworkError& e) {
		error_dialog (this, wxString::Format (_("Could not get country list (%s)"), e.what()));
		_country->Clear ();
	}
}

void
DolbyCertificatePanel::country_selected ()
{
	_cinema->Clear ();
	_cinema->Append (_("Fetching..."));
	_cinema->SetSelection (0);

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif
	signal_manager->when_idle (boost::bind (&DolbyCertificatePanel::finish_country_selected, this));
}

void
DolbyCertificatePanel::finish_country_selected ()
{
	try {
		list<string> const c = get_dir (wx_to_std (_country->GetStringSelection()));
		_cinema->Clear ();
		BOOST_FOREACH (string i, c) {
			_cinema->Append (std_to_wx (i));
		}
	} catch (NetworkError& e) {
		error_dialog (this, wxString::Format (_("Could not get cinema list (%s)"), e.what ()));
		_cinema->Clear ();
	}
}

void
DolbyCertificatePanel::cinema_selected ()
{
	_serial->Clear ();
	_serial->Append (_("Fetching..."));
	_serial->SetSelection (0);

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif
	signal_manager->when_idle (boost::bind (&DolbyCertificatePanel::finish_cinema_selected, this));
}

void
DolbyCertificatePanel::finish_cinema_selected ()
{
	try {
		list<string> const s = get_dir (String::compose ("%1/%2", wx_to_std (_country->GetStringSelection()), wx_to_std (_cinema->GetStringSelection())));
		_serial->Clear ();
		BOOST_FOREACH (string i, s) {
			vector<string> a;
			split (a, i, is_any_of ("-_"));
			if (a.size() >= 4) {
				_serial->Append (std_to_wx (a[3]), new wxStringClientData (std_to_wx (i)));
			}
		}
	} catch (NetworkError& e) {
		error_dialog (this, wxString::Format (_("Could not get screen list (%s)"), e.what()));
		_serial->Clear ();
	}
}

void
DolbyCertificatePanel::download (wxStaticText* message)
{
	message->SetLabel (_("Downloading certificate"));

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif

	signal_manager->when_idle (boost::bind (&DolbyCertificatePanel::finish_download, this, message));
}

void
DolbyCertificatePanel::finish_download (wxStaticText* message)
{
	string const zip = string_client_data (_serial->GetClientObject (_serial->GetSelection ()));

	string const file = String::compose (
		"ftp://dolbyrootcertificates:houro61l@ftp.dolby.co.uk/SHA256/%1/%2/%3",
		wx_to_std (_country->GetStringSelection()),
		wx_to_std (_cinema->GetStringSelection()),
		zip
		);

	/* Work out the certificate file name inside the zip */
	vector<string> b;
	split (b, zip, is_any_of ("_"));
	if (b.size() < 2) {
		message->SetLabel (_("Unexpected certificate filename form"));
		return;
	}
	string const cert = b[0] + "_" + b[1] + ".pem.crt";

	optional<string> error = get_from_zip_url (file, cert, boost::bind (&DownloadCertificatePanel::load, this, _1));
	if (error) {
		message->SetLabel (std_to_wx (error.get ()));
	} else {
		message->SetLabel (_("Certificate downloaded"));
	}
}

bool
DolbyCertificatePanel::ready_to_download () const
{
	/* XXX */
	return false;
}

void
DolbyCertificatePanel::setup ()
{
	signal_manager->when_idle (boost::bind (&DolbyCertificatePanel::setup_countries, this));
}
