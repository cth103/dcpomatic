/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "dolby_doremi_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/util.h"
#include "lib/signal_manager.h"
#include "lib/internet.h"
#include <dcp/raw_convert.h>
#include <curl/curl.h>
#include <zip.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;
using std::cout;
using std::list;
using boost::function;
using boost::optional;
using dcp::raw_convert;

DolbyDoremiCertificatePanel::DolbyDoremiCertificatePanel (wxWindow* parent, DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (parent, dialog)
{
	add_label_to_sizer (_table, this, _("Serial number"), true);
	_serial = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (300, -1));
	_table->Add (_serial, 1, wxEXPAND);

	_serial->Bind (wxEVT_TEXT, boost::bind (&DownloadCertificateDialog::setup_sensitivity, _dialog));

	layout ();
}

void
DolbyDoremiCertificatePanel::download (wxStaticText* message)
{
	message->SetLabel (_("Downloading certificate"));

	/* Hack: without this the SetLabel() above has no visible effect */
	wxMilliSleep (200);

	signal_manager->when_idle (boost::bind (&DolbyDoremiCertificatePanel::finish_download, this, wx_to_std (_serial->GetValue ()), message));
}

static void
try_dcp2000 (list<string>& urls, list<string>& files, string prefix, string serial)
{
	urls.push_back (String::compose ("%1%2xxx/dcp2000-%3.dcicerts.zip", prefix, serial.substr(0, 3), serial));
	files.push_back (String::compose ("dcp2000-%1.cert.sha256.pem", serial));

	urls.push_back (String::compose ("%1%2xxx/dcp2000-%3.dcicerts.zip", prefix, serial.substr(0, 3), serial));
	files.push_back (String::compose ("dcp2000-%1.cert.sha256.pem", serial));

	urls.push_back (String::compose ("%1%2xxx/dcp2000-%3.certs.zip", prefix, serial.substr(0, 3), serial));
	files.push_back (String::compose ("dcp2000-%1.cert.sha256.pem", serial));
}

static void
try_imb (list<string>& urls, list<string>& files, string prefix, string serial)
{
	urls.push_back (String::compose ("%1%2xxx/imb-%3.dcicerts.zip", prefix, serial.substr(0, 3), serial));
	files.push_back (String::compose ("imb-%1.cert.sha256.pem", serial));
}

static void
try_ims (list<string>& urls, list<string>& files, string prefix, string serial)
{
	urls.push_back (String::compose ("%1%2xxx/ims-%3.dcicerts.zip", prefix, serial.substr(0, 3), serial));
	files.push_back (String::compose ("ims-%1.cert.sha256.pem", serial));
}

static void
try_cat862 (list<string>& urls, list<string>& files, string prefix, string serial)
{
	int const serial_int = raw_convert<int> (serial);

	string cat862;
	if (serial_int <= 510999) {
		cat862 = "CAT862_510999_and_lower";
	} else if (serial_int >= 617000) {
		cat862 = "CAT862_617000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		cat862 = String::compose ("CAT862_%1-%2", lower, lower + 999);
	}

	urls.push_back (String::compose ("%1%2/cert_Dolby256-CAT862-%3.zip", prefix, cat862, serial_int));
	files.push_back (String::compose ("cert_Dolby256-CAT862-%1.pem.crt", serial_int));
}

static void
try_dsp100 (list<string>& urls, list<string>& files, string prefix, string serial)
{
	int const serial_int = raw_convert<int> (serial);

	string dsp100;
	if (serial_int <= 999) {
		dsp100 = "DSP100_053_thru_999";
	} else if (serial_int >= 3000) {
		dsp100 = "DSP100_3000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		dsp100 = String::compose ("DSP100_%1_thru_%2", lower, lower + 999);
	}

	urls.push_back (String::compose ("%1%2/cert_Dolby256-DSP100-%3.zip", prefix, dsp100, serial_int));
	files.push_back (String::compose ("cert_Dolby256-DSP100-%1.pem.crt", serial_int));
}

static void
try_cat745 (list<string>& urls, list<string>& files, string prefix, string serial)
{
	int const serial_int = raw_convert<int> (serial.substr (1));

	string cat745;
	if (serial_int <= 999) {
		cat745 = "CAT745_1_thru_999";
	} else if (serial_int >= 6000) {
		cat745 = "CAT745_6000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		cat745 = String::compose ("CAT745_%1_thru_%2", lower, lower + 999);
	}

	urls.push_back (String::compose ("%1%2/cert_Dolby-CAT745-%3.zip", prefix, cat745, serial_int));
	files.push_back (String::compose ("cert_Dolby-CAT745-%1.pem.crt", serial_int));
}

static void
try_cp850 (list<string>& urls, list<string>& files, string prefix, string serial)
{
	int const serial_int = raw_convert<int> (serial.substr (1));

	int const lower = serial_int - (serial_int % 1000);
	urls.push_back (String::compose ("%1CP850_CAT1600_F%2-F%3/cert_RMB_SPB_MDE_FMA.Dolby-CP850-F%4.zip", prefix, lower, lower + 999, serial_int));
	files.push_back (String::compose ("cert_RMB_SPB_MDE_FMA.Dolby-CP850-F%1.pem.crt", serial_int));
}

void
DolbyDoremiCertificatePanel::finish_download (string serial, wxStaticText* message)
{
	/* Try dcp2000, imb and ims prefixes (see mantis #375) */

	string const prefix = "ftp://anonymous@ftp.cinema.dolby.com/Certificates/";
	list<string> urls;
	list<string> files;

	bool starts_with_digit = false;
	optional<char> starting_char;

	if (!serial.empty()) {
		if (isdigit (serial[0])) {
			starts_with_digit = true;
		} else {
			starting_char = serial[0];
		}
	}

	if (starts_with_digit) {
		try_dcp2000 (urls, files, prefix, serial);
		try_imb (urls, files, prefix, serial);
		try_ims (urls, files, prefix, serial);
		try_cat862 (urls, files, prefix, serial);
		try_dsp100 (urls, files, prefix, serial);
	} else if (starting_char == 'H') {
		try_cat745 (urls, files, prefix, serial);
	} else if (starting_char == 'F') {
		try_cp850 (urls, files, prefix, serial);
	}

	list<string> errors;
	bool ok = false;
	list<string>::const_iterator i = urls.begin ();
	list<string>::const_iterator j = files.begin ();
	while (!ok && i != urls.end ()) {
		optional<string> error = get_from_zip_url (*i++, *j++, true, boost::bind (&DownloadCertificatePanel::load, this, _1));
		if (error) {
			errors.push_back (error.get ());
		} else {
			ok = true;
		}
	}

	if (ok) {
		message->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	} else {
		message->SetLabel (wxT (""));

		string s;
		BOOST_FOREACH (string e, errors) {
			s += e + "\n";
		}

		error_dialog (this, std_to_wx (s));
	}
}

bool
DolbyDoremiCertificatePanel::ready_to_download () const
{
	return !_serial->IsEmpty ();
}
