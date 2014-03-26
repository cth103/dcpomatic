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
#include <zip.h>
#include "lib/compose.hpp"
#include "lib/util.h"
#include "doremi_certificate_dialog.h"
#include "wx_util.h"

using std::string;
using boost::function;

DoremiCertificateDialog::DoremiCertificateDialog (wxWindow* parent, function<void (boost::filesystem::path)> load)
	: DownloadCertificateDialog (parent, load)
{
	add (_("Server serial number"), true);
	_serial = add (new wxTextCtrl (this, wxID_ANY));

	add_common_widgets ();
}



static size_t
ftp_data (void* buffer, size_t size, size_t nmemb, void* stream)
{
	FILE* f = reinterpret_cast<FILE*> (stream);
	return fwrite (buffer, size, nmemb, f);
}

void
DoremiCertificateDialog::download ()
{
	string const serial = wx_to_std (_serial->GetValue ());
	if (serial.length() != 6) {
		_message->SetLabel (_("Doremi serial numbers must have 6 digits"));
		return;
	}
	
	CURL* curl = curl_easy_init ();
	if (!curl) {
		_message->SetLabel (N_("Could not set up libcurl"));
		return;
	}
	
	string const url = String::compose (
		"ftp://service:t3chn1c1an@ftp.doremilabs.com/Certificates/%1xxx/dcp2000-%2.dcicerts.zip",
		serial.substr(0, 3), serial
		);
	
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
	
	ScopedTemporary temp_zip;
	FILE* f = temp_zip.open ("wb");
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ftp_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, f);

	_message->SetLabel (_("Downloading certificate from Doremi"));
	run_gui_loop ();

	CURLcode const cr = curl_easy_perform (curl);

	_gauge->SetValue (50);
	run_gui_loop ();
	
	temp_zip.close ();
	curl_easy_cleanup (curl);
	if (cr != CURLE_OK) {
		_message->SetLabel (wxString::Format (_("Certificate download failed (%d)"), cr));
		return;
	}
	
	_message->SetLabel (_("Unpacking"));
	run_gui_loop ();
	
	struct zip* zip = zip_open (temp_zip.c_str(), 0, 0);
	if (!zip) {
		_message->SetLabel (N_("Could not open certificate ZIP file"));
		return;
	}
	
	string const name_in_zip = String::compose ("dcp2000-%1.cert.sha256.pem", serial);
	struct zip_file* zip_file = zip_fopen (zip, name_in_zip.c_str(), 0);
	if (!zip_file) {
		_message->SetLabel (N_("Could not find certificate in ZIP file"));
		return;
	}
	
	ScopedTemporary temp_cert;
	f = temp_cert.open ("wb");
	char buffer[4096];
	while (1) {
		int const N = zip_fread (zip_file, buffer, sizeof (buffer));
		fwrite (buffer, 1, N, f);
		if (N < int (sizeof (buffer))) {
			break;
		}
	}
	temp_cert.close ();
	
	_gauge->SetValue (100);
	_message->SetLabel (_("OK"));
	_load (temp_cert.file ());
}
