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

#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include "lib/compose.hpp"
#include "lib/internet.h"
#include "lib/signal_manager.h"
#include "dolby_certificate_dialog.h"
#include "wx_util.h"

using std::list;
using std::string;
using std::vector;
using std::cout;
using boost::optional;
using boost::algorithm::split;
using boost::algorithm::is_any_of;

DolbyCertificateDialog::DolbyCertificateDialog (wxWindow* parent, boost::function<void (boost::filesystem::path)> load)
	: DownloadCertificateDialog (parent, load)
{
	add (_("Country"), true);
	_country = add (new wxChoice (this, wxID_ANY));
	_country->Append (N_("Hashemite Kingdom of Jordan"));
	
	add (_("Cinema"), true);
	_cinema = add (new wxChoice (this, wxID_ANY));
	_cinema->Append (N_("Motion Picture Solutions London Mobile & QC"));

	add (_("Serial number"), true);
	_serial = add (new wxChoice (this, wxID_ANY));

	add_common_widgets ();

	_country->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificateDialog::country_selected, this));
	_cinema->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificateDialog::cinema_selected, this));
	_serial->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificateDialog::serial_selected, this));
	Bind (wxEVT_IDLE, boost::bind (&DolbyCertificateDialog::setup_countries, this));

	_country->Clear ();
	_cinema->Clear ();
}

list<string>
DolbyCertificateDialog::get_dir (string dir) const
{
	string url = String::compose ("ftp://dolbyrootcertificates:houro61l@ftp.dolby.co.uk/SHA256/%1", dir);
	return ftp_ls (url);
}

void
DolbyCertificateDialog::setup_countries ()
{
	if (_country->GetCount() > 0) {
		/* Already set up */
		return;
	}
	
	_country->Append (_("Fetching..."));
	_country->SetSelection (0);

#ifdef DCPOMATIC_OSX
	/* See DoremiCertificateDialog for discussion about this daft delay */
	wxMilliSleep (200);
#endif
	signal_manager->when_idle (boost::bind (&DolbyCertificateDialog::finish_setup_countries, this));
}

void
DolbyCertificateDialog::finish_setup_countries ()
{
	list<string> const countries = get_dir ("");
	_country->Clear ();
	for (list<string>::const_iterator i = countries.begin(); i != countries.end(); ++i) {
		_country->Append (std_to_wx (*i));
	}
}

void
DolbyCertificateDialog::country_selected ()
{
	_cinema->Clear ();
	_cinema->Append (_("Fetching..."));
	_cinema->SetSelection (0);

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif	
	signal_manager->when_idle (boost::bind (&DolbyCertificateDialog::finish_country_selected, this));
}

void
DolbyCertificateDialog::finish_country_selected ()
{
	list<string> const cinemas = get_dir (wx_to_std (_country->GetStringSelection()));
	_cinema->Clear ();
	for (list<string>::const_iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
		_cinema->Append (std_to_wx (*i));
	}
}

void
DolbyCertificateDialog::cinema_selected ()
{
	_serial->Clear ();
	_serial->Append (_("Fetching..."));
	_serial->SetSelection (0);

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif
	signal_manager->when_idle (boost::bind (&DolbyCertificateDialog::finish_cinema_selected, this));
}

void
DolbyCertificateDialog::finish_cinema_selected ()
{
	string const dir = String::compose ("%1/%2", wx_to_std (_country->GetStringSelection()), wx_to_std (_cinema->GetStringSelection()));
	list<string> const zips = get_dir (dir);

	_serial->Clear ();
	for (list<string>::const_iterator i = zips.begin(); i != zips.end(); ++i) {
		vector<string> a;
		split (a, *i, is_any_of ("-_"));
		if (a.size() >= 4) {
			_serial->Append (std_to_wx (a[3]), new wxStringClientData (std_to_wx (*i)));
		}
	}
}

void
DolbyCertificateDialog::serial_selected ()
{
	_download->Enable (true);
}

void
DolbyCertificateDialog::download ()
{
	downloaded (false);
	_message->SetLabel (_("Downloading certificate"));

#ifdef DCPOMATIC_OSX
	wxMilliSleep (200);
#endif

	signal_manager->when_idle (boost::bind (&DolbyCertificateDialog::finish_download, this));
}

void
DolbyCertificateDialog::finish_download ()
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
		_message->SetLabel (_("Unexpected certificate filename form"));
		return;
	}
	string const cert = b[0] + "_" + b[1] + ".pem.crt";

	optional<string> error = get_from_zip_url (file, cert, _load);
	if (error) {
		_message->SetLabel (std_to_wx (error.get ()));
	} else {
		_message->SetLabel (_("Certificate downloaded"));
		downloaded (true);
	}
}
