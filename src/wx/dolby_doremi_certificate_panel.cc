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


#include "dolby_doremi_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/internet.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include <dcp/raw_convert.h>
#include <curl/curl.h>
#include <zip.h>
#include <boost/algorithm/string.hpp>


using std::function;
using std::string;
using std::vector;
using namespace boost::algorithm;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;


class Location
{
public:
	Location(string url_, string file_)
		: url(url_)
		, file(file_)
	{}

	string url;
	string file;
};


DolbyDoremiCertificatePanel::DolbyDoremiCertificatePanel (DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel (dialog)
{

}


static void
try_common(vector<Location>& locations, string prefix, string serial)
{
	auto files = ls_url(String::compose("%1%2xxx/", prefix, serial.substr(0, 3)));

	auto check = [&locations, prefix, files, serial](string format, string file) {
		auto const zip = String::compose(format, serial);
		if (find(files.begin(), files.end(), zip) != files.end()) {
			locations.push_back({
				String::compose("%1%2xxx/%3", prefix, serial.substr(0, 3), zip),
				String::compose(file, serial)
			});
		}
	};

	check("Dolby-DCP2000-%1.dcicerts.zip", "Dolby-DCP2000-%1.cert.sha256.pem");
	check("Dolby-DCP2000-%1.certs.zip", "Dolby-DCP2000-%1.cert.sha256.pem");
	check("dcp2000-%1.dcicerts.zip", "dcp2000-%1.cert.sha256.pem");
	check("dcp2000-%1.certs.zip", "dcp2000-%1.cert.sha256.pem");
	check("Dolby-IMB-%1.dcicerts.zip", "Dolby-IMB-%1.cert.sha256.pem");
	check("imb-%1.dcicerts.zip", "imb-%1.cert.sha256.pem");
	check("Dolby-IMS1000-%1.dcicerts.zip", "Dolby-IMS1000-%1.cert.sha256.pem");
	check("Dolby-IMS2000-%1.dcicerts.zip", "Dolby-IMS2000-%1.cert.sha256.pem");
	check("cert_Dolby-IMS3000-%1-SMPTE.zip", "cert_Dolby-IMS3000-%1-SMPTE.pem");
	check("ims-%1.dcicerts.zip", "ims-%1.cert.sha256.pem");
}


static void
try_cat862(vector<Location>& locations, string prefix, string serial)
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

	locations.push_back({
		String::compose("%1%2/cert_Dolby256-CAT862-%3.zip", prefix, cat862, serial_int),
		String::compose("cert_Dolby256-CAT862-%1.pem.crt", serial_int)
	});
}


static void
try_dsp100(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial);

	string dsp100;
	if (serial_int <= 999) {
		dsp100 = "DSP100_053_thru_999";
	} else if (serial_int >= 3000) {
		dsp100 = "DSP100_3000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		dsp100 = String::compose ("DSP100_%1_thru_%2", lower, lower + 999);
	}

	locations.push_back({
		String::compose("%1%2/cert_Dolby256-DSP100-%3.zip", prefix, dsp100, serial_int),
		String::compose("cert_Dolby256-DSP100-%1.pem.crt", serial_int)
	});
}


static void
try_cat745(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial.substr (1));

	string cat745;
	if (serial_int <= 999) {
		cat745 = "CAT745_1_thru_999";
	} else if (serial_int >= 6000) {
		cat745 = "CAT745_6000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		cat745 = String::compose("CAT745_%1_thru_%2", lower, lower + 999);
	}

	locations.push_back({
		String::compose("%1%2/cert_Dolby-CAT745-%3.zip", prefix, cat745, serial_int),
		String::compose("cert_Dolby-CAT745-%1.pem.crt", serial_int)
	});
}


static void
try_cp850(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int> (serial.substr (1));

	int const lower = serial_int - (serial_int % 1000);
	locations.push_back({
		String::compose ("%1CP850_CAT1600_F%2-F%3/cert_RMB_SPB_MDE_FMA.Dolby-CP850-F%4.zip", prefix, lower, lower + 999, serial_int),
		String::compose ("cert_RMB_SPB_MDE_FMA.Dolby-CP850-F%1.pem.crt", serial_int)
	});
}


void
DolbyDoremiCertificatePanel::do_download ()
{
	string serial = wx_to_std(_serial->GetValue());
	trim(serial);

	/* Try dcp2000, imb and ims prefixes (see mantis #375) */

	string const prefix = "ftp://ftp.cinema.dolby.com/Certificates/";
	vector<Location> locations;

	bool starts_with_digit = false;
	optional<char> starting_char;

	if (!serial.empty()) {
		if (isdigit (serial[0])) {
			starts_with_digit = true;
		} else {
			starting_char = serial[0];
		}
	}

	vector<string> errors;

	if (starts_with_digit) {
		try_common(locations, prefix, serial);
		try_cat862(locations, prefix, serial);
		try_dsp100(locations, prefix, serial);
	} else if (starting_char == 'H') {
		try_cat745(locations, prefix, serial);
	} else if (starting_char == 'F') {
		try_cp850(locations, prefix, serial);
	} else {
		errors.push_back(wx_to_std(_("Unrecognised serial number format (does not start with a number, H or F)")));
	}

	bool ok = false;
	auto location = locations.begin();
	while (!ok && location != locations.end()) {
		auto error = get_from_zip_url(location->url, location->file, true, true, boost::bind(&DownloadCertificatePanel::load_certificate, this, _1, _2));
		++location;
		if (error) {
			errors.push_back (error.get ());
		} else {
			ok = true;
		}
	}

	if (ok) {
		_dialog->message()->SetLabel (_("Certificate downloaded"));
		_dialog->setup_sensitivity ();
	} else {
		_dialog->message()->SetLabel (wxT (""));

		string s;
		for (auto e: errors) {
			s += e + "\n";
		}

		error_dialog (this, std_to_wx (s));
	}
}


wxString
DolbyDoremiCertificatePanel::name () const
{
	return _("Dolby / Doremi");
}
