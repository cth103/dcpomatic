/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <wx/filepicker.h>
#include <wx/validate.h>
#include <curl/curl.h>
#include <zip.h>
#include <libdcp/exceptions.h>
#include "lib/compose.hpp"
#include "lib/util.h"
#include "screen_dialog.h"
#include "wx_util.h"
#include "progress.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ScreenDialog::ScreenDialog (wxWindow* parent, string title, string name, shared_ptr<libdcp::Certificate> certificate)
	: wxDialog (parent, wxID_ANY, std_to_wx (title))
	, _certificate (certificate)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, "Name", true);
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (320, -1));
	table->Add (_name, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Server manufacturer", true);
	_manufacturer = new wxChoice (this, wxID_ANY);
	table->Add (_manufacturer, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Server serial number", true);
	_serial = new wxTextCtrl (this, wxID_ANY);
	table->Add (_serial, 1, wxEXPAND);
	
	add_label_to_sizer (table, this, "Certificate", true);
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	_load_certificate = new wxButton (this, wxID_ANY, _("Load from file..."));
	_download_certificate = new wxButton (this, wxID_ANY, _("Download"));
	s->Add (_load_certificate, 1, wxEXPAND);
	s->Add (_download_certificate, 1, wxEXPAND);
	table->Add (s, 1, wxEXPAND);

	table->AddSpacer (0);
	_progress = new Progress (this);
	table->Add (_progress, 1, wxEXPAND);

	table->AddSpacer (0);
	_certificate_text = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (320, 256), wxTE_MULTILINE | wxTE_READONLY);
	if (certificate) {
		_certificate_text->SetValue (certificate->certificate ());
	}
	wxFont font = wxSystemSettings::GetFont (wxSYS_ANSI_FIXED_FONT);
	font.SetPointSize (font.GetPointSize() / 2);
	_certificate_text->SetFont (font);
	table->Add (_certificate_text, 1, wxEXPAND);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_manufacturer->Append (_("Unknown"));
	_manufacturer->Append (_("Doremi"));
	_manufacturer->Append (_("Other"));
	_manufacturer->SetSelection (0);

	_load_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::select_certificate, this));
	_download_certificate->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreenDialog::download_certificate, this));
	_manufacturer->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ScreenDialog::setup_sensitivity, this));
	_serial->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ScreenDialog::setup_sensitivity, this));

	setup_sensitivity ();
}

string
ScreenDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

shared_ptr<libdcp::Certificate>
ScreenDialog::certificate () const
{
	return _certificate;
}

void
ScreenDialog::load_certificate (boost::filesystem::path file)
{
	try {
		_certificate.reset (new libdcp::Certificate (file));
		_certificate_text->SetValue (_certificate->certificate ());
	} catch (libdcp::MiscError& e) {
		error_dialog (this, String::compose ("Could not read certificate file (%1)", e.what()));
	}
}

void
ScreenDialog::select_certificate ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select Certificate File"));

	if (d->ShowModal () == wxID_OK) {
		load_certificate (boost::filesystem::path (wx_to_std (d->GetPath ())));
	}
	
	d->Destroy ();

	setup_sensitivity ();
}

static size_t
ftp_data (void* buffer, size_t size, size_t nmemb, void* stream)
{
	FILE* f = reinterpret_cast<FILE*> (stream);
	return fwrite (buffer, size, nmemb, f);
}

void
ScreenDialog::download_certificate ()
{
	if (_manufacturer->GetStringSelection() == _("Doremi")) {
		string const serial = wx_to_std (_serial->GetValue ());
		if (serial.length() != 6) {
			error_dialog (this, _("Doremi serial numbers must have 6 numbers"));
			return;
		}

		CURL* curl = curl_easy_init ();
		if (!curl) {
			error_dialog (this, N_("Could not set up libcurl"));
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
		_progress->set_message (_("Downloading certificate from Doremi"));
		CURLcode const cr = curl_easy_perform (curl);
		_progress->set_value (50);
		temp_zip.close ();
		curl_easy_cleanup (curl);
		if (cr != CURLE_OK) {
			_progress->set_message (wxString::Format (_("Certificate download failed (%d)"), cr));
			return;
		}

		_progress->set_message (_("Unpacking"));
		struct zip* zip = zip_open (temp_zip.c_str(), 0, 0);
		if (!zip) {
			_progress->set_message ("Could not open certificate ZIP file");
			return;
		}

		string const name_in_zip = String::compose ("dcp2000-%1.cert.sha256.pem", serial);
		struct zip_file* zip_file = zip_fopen (zip, name_in_zip.c_str(), 0);
		if (!zip_file) {
			_progress->set_message ("Could not find certificate in ZIP file");
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

		_progress->set_value (100);
		_progress->set_message (_("OK"));
		load_certificate (temp_cert.file ());
	}
}

void
ScreenDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton*> (FindWindowById (wxID_OK, this));
	ok->Enable (_certificate);

	_download_certificate->Enable (_manufacturer->GetStringSelection() == _("Doremi") && !_serial->GetValue().IsEmpty ());
}
