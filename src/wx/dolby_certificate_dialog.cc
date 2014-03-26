/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <curl/curl.h>
#include "lib/compose.hpp"
#include "dolby_certificate_dialog.h"
#include "wx_util.h"

using std::list;
using std::string;
using std::stringstream;
using std::cout;

DolbyCertificateDialog::DolbyCertificateDialog (wxWindow* parent, boost::function<void (boost::filesystem::path)> load)
	: DownloadCertificateDialog (parent, load)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	_country = new wxChoice (this, wxID_ANY);
	add_label_to_sizer (table, this, _("Country"), true);
	table->Add (_country, 1, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_country->Append (N_("Hashemite Kingdom of Jordan"));
	
	_cinema = new wxChoice (this, wxID_ANY);
	add_label_to_sizer (table, this, _("Cinema"), true);
	table->Add (_cinema, 1, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_cinema->Append (N_("Wometco Dominicana Palacio Del Cine"));
	_overall_sizer->Add (table);

	add_common_widgets ();

	_country->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificateDialog::country_selected, this));
	_cinema->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DolbyCertificateDialog::cinema_selected, this));

	_country->Clear ();
	_cinema->Clear ();
}

static size_t
ftp_data_ls (void* buffer, size_t size, size_t nmemb, void* data)
{
	string* s = reinterpret_cast<string *> (data);
	uint8_t* b = reinterpret_cast<uint8_t *> (buffer);
	for (size_t i = 0; i < (size * nmemb); ++i) {
		*s += b[i];
	}
	return nmemb;
}

list<string>
DolbyCertificateDialog::ftp_ls (string dir) const
{
	CURL* curl = curl_easy_init ();
	if (!curl) {
		_message->SetLabel (N_("Could not set up libcurl"));
		return list<string> ();
	}

	string url = String::compose ("ftp://dolbyrootcertificates:houro61l@ftp.dolby.co.uk/SHA256/%1", dir);
	if (url.substr (url.length() - 1, 1) != "/") {
		url += "/";
	}
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());

	string ls_raw;
	struct curl_slist* commands = 0;
	commands = curl_slist_append (commands, "NLST");
	curl_easy_setopt (curl, CURLOPT_POSTQUOTE, commands);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ls_raw);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ftp_data_ls);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPSV, 0);
	CURLcode const r = curl_easy_perform (curl);
	if (r != CURLE_OK) {
		_message->SetLabel (_("Problem occurred when contacting Dolby."));
		return list<string> ();
	}

	stringstream s (ls_raw);
	string line;
	list<string> ls;
	while (s.good ()) {
		getline (s, line);
		if (line.length() > 55) {
			string const file = line.substr (55);
			if (file != "." && file != "..") {
				ls.push_back (file);
			}
		}
	}

	curl_easy_cleanup (curl);

	return ls;
}

void
DolbyCertificateDialog::setup ()
{
	_message->SetLabel (_("Fetching available countries"));
	run_gui_loop ();
	list<string> const countries = ftp_ls ("");
	for (list<string>::const_iterator i = countries.begin(); i != countries.end(); ++i) {
		_country->Append (std_to_wx (*i));
	}
	_message->SetLabel ("");
}

void
DolbyCertificateDialog::country_selected ()
{
	_message->SetLabel (_("Fetching available cinemas"));
	run_gui_loop ();
	list<string> const cinemas = ftp_ls (wx_to_std (_country->GetStringSelection()));
	_cinema->Clear ();
	for (list<string>::const_iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
		_cinema->Append (std_to_wx (*i));
	}
	_message->SetLabel ("");
}

void
DolbyCertificateDialog::cinema_selected ()
{
	_download->Enable (true);
}

void
DolbyCertificateDialog::download ()
{

}
