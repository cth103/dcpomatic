/*
    Copyright (C) 2026 Jaume Puig

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


#include "certificate_download.h"
#include "config.h"
#include "internet.h"
#include <dcp/certificate.h>
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/util.h>
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <stdexcept>


using std::list;
using std::runtime_error;
using std::string;
using std::vector;
using boost::optional;


static
int
serial_to_int(string serial)
{
	try {
		size_t used = 0;
		auto const value = std::stoi(serial, &used);
		if (used == serial.length()) {
			return value;
		}
	} catch (std::exception&) {}
	throw runtime_error("invalid serial number");
}


/** ftp.cinema.dolby.com keeps the certificates of some devices in ZIP files whose names are not predictable */
static void
dolby_common(vector<CertificateSource>& sources, string prefix, string serial, std::function<list<string> (string)> ls)
{
	auto const directory = fmt::format("{}{}xxx/", prefix, serial.substr(0, 3));
	auto const files = ls(directory);

	auto check = [&sources, directory, files, serial](string format, string file) {
		auto const zip = fmt::format(format, serial);
		if (std::find(files.begin(), files.end(), zip) != files.end()) {
			sources.push_back({directory + zip, fmt::format(file, serial)});
		}
	};

	check("Dolby-DCP2000-{}.dcicerts.zip", "Dolby-DCP2000-{}.cert.sha256.pem");
	check("Dolby-DCP2000-{}.certs.zip", "Dolby-DCP2000-{}.cert.sha256.pem");
	check("dcp2000-{}.dcicerts.zip", "dcp2000-{}.cert.sha256.pem");
	check("dcp2000-{}.certs.zip", "dcp2000-{}.cert.sha256.pem");
	check("Dolby-IMB-{}.dcicerts.zip", "Dolby-IMB-{}.cert.sha256.pem");
	check("imb-{}.dcicerts.zip", "imb-{}.cert.sha256.pem");
	check("Dolby-IMS1000-{}.dcicerts.zip", "Dolby-IMS1000-{}.cert.sha256.pem");
	check("Dolby-IMS2000-{}.dcicerts.zip", "Dolby-IMS2000-{}.cert.sha256.pem");
	check("cert_Dolby-IMS3000-{}-SMPTE.zip", "cert_Dolby-IMS3000-{}-SMPTE.pem");
	check("ims-{}.dcicerts.zip", "ims-{}.cert.sha256.pem");
}


static void
dolby_cat862(vector<CertificateSource>& sources, string prefix, int serial)
{
	string folder;
	if (serial <= 510999) {
		folder = "CAT862_510999_and_lower";
	} else if (serial >= 617000) {
		folder = "CAT862_617000_and_higher";
	} else {
		int const lower = serial - (serial % 1000);
		folder = fmt::format("CAT862_{}-{}", lower, lower + 999);
	}
	sources.push_back({
		fmt::format("{}{}/cert_Dolby256-CAT862-{}.zip", prefix, folder, serial),
		fmt::format("cert_Dolby256-CAT862-{}.pem.crt", serial)
	});
}


static void
dolby_dsp100(vector<CertificateSource>& sources, string prefix, int serial)
{
	string folder;
	if (serial <= 999) {
		folder = "DSP100_053_thru_999";
	} else if (serial >= 3000) {
		folder = "DSP100_3000_and_higher";
	} else {
		int const lower = serial - (serial % 1000);
		folder = fmt::format("DSP100_{}_thru_{}", lower, lower + 999);
	}
	sources.push_back({
		fmt::format("{}{}/cert_Dolby256-DSP100-{}.zip", prefix, folder, serial),
		fmt::format("cert_Dolby256-DSP100-{}.pem.crt", serial)
	});
}


static void
dolby_cat745(vector<CertificateSource>& sources, string prefix, int serial)
{
	string folder;
	if (serial <= 999) {
		folder = "CAT745_1_thru_999";
	} else if (serial >= 6000) {
		folder = "CAT745_6000_and_higher";
	} else {
		int const lower = serial - (serial % 1000);
		folder = fmt::format("CAT745_{}_thru_{}", lower, lower + 999);
	}
	sources.push_back({
		fmt::format("{}{}/cert_Dolby-CAT745-{}.zip", prefix, folder, serial),
		fmt::format("cert_Dolby-CAT745-{}.pem.crt", serial)
	});
}


static void
dolby_cp850(vector<CertificateSource>& sources, string prefix, int serial)
{
	int const lower = serial - (serial % 1000);
	sources.push_back({
		fmt::format("{}CP850_CAT1600_F{}-F{}/cert_RMB_SPB_MDE_FMA.Dolby-CP850-F{}.zip", prefix, lower, lower + 999, serial),
		fmt::format("cert_RMB_SPB_MDE_FMA.Dolby-CP850-F{}.pem.crt", serial)
	});
}


static void
qube(vector<CertificateSource>& sources, string type, string serial, std::function<list<string> (string)> ls)
{
	string const base = "ftp://certificates.qubecinema.com/";
	auto const directory = fmt::format("{}SMPTE-{}/", base, type);
	auto const files = ls(directory);
	auto const start = fmt::format("{}-{}-", type, serial);
	for (auto const& file: files) {
		if (boost::algorithm::starts_with(file, start)) {
			sources.push_back({directory + file});
			return;
		}
	}
}


vector<CertificateSource>
certificate_sources(string maker, string serial, std::function<list<string> (string)> ls)
{
	boost::algorithm::trim(serial);
	boost::algorithm::to_lower(maker);

	if (serial.empty() || serial.find_first_of("/\\ :@") != string::npos) {
		throw runtime_error("invalid serial number");
	}

	auto config = Config::instance();
	vector<CertificateSource> sources;

	if (maker == "gdc") {
		if (!config->gdc_username() || !config->gdc_password()) {
			throw runtime_error("the GDC user name and password are not set in the configuration");
		}
		sources.push_back({fmt::format("ftp://{}:{}@ftp.gdc-tech.com/SHA256/{}.crt.pem", *config->gdc_username(), *config->gdc_password(), serial)});
	} else if (maker == "barco") {
		if (serial.length() != 10) {
			throw runtime_error("a Barco serial number has 10 characters");
		}
		if (!config->barco_username() || !config->barco_password()) {
			throw runtime_error("the Barco user name and password are not set in the configuration");
		}
		sources.push_back({fmt::format(
			"sftp://{}:{}@certificates.barco.com/{}xxx/{}/Barco-ICMP.{}_cert.pem",
			*config->barco_username(), *config->barco_password(), serial.substr(0, 7), serial, serial
			)});
	} else if (maker == "christie") {
		if (!config->christie_username() || !config->christie_password()) {
			throw runtime_error("the Christie user name and password are not set in the configuration");
		}
		if (serial.length() > 12) {
			throw runtime_error("a Christie serial number has at most 12 characters");
		}
		serial.insert(0, 12 - serial.length(), '0');
		auto const prefix = fmt::format(
			"ftp://{}:{}@certificates.christiedigital.com/Certificates/", *config->christie_username(), *config->christie_password()
			);
		sources.push_back({fmt::format("{}F-IMB/F-IMB_{}_sha256.pem", prefix, serial), {}, true});
		sources.push_back({fmt::format("{}IMB-S2/IMB-S2_{}_sha256.pem", prefix, serial), {}, true});
	} else if (maker == "qube" || maker == "qxi" || maker == "qxpd") {
		for (string type: {"QXI", "QXPD"}) {
			if (maker == "qube" || boost::algorithm::to_upper_copy(maker) == type) {
				qube(sources, type, serial, ls);
			}
		}
		if (sources.empty()) {
			throw runtime_error(fmt::format("could not find serial number {} on the Qube server", serial));
		}
	} else if (maker == "dolby" || maker == "doremi") {
		string const prefix = "ftp://ftp.cinema.dolby.com/Certificates/";
		if (isdigit(serial[0])) {
			dolby_common(sources, prefix, serial, ls);
			auto const number = serial_to_int(serial);
			dolby_cat862(sources, prefix, number);
			dolby_dsp100(sources, prefix, number);
		} else if (serial[0] == 'H') {
			dolby_cat745(sources, prefix, serial_to_int(serial.substr(1)));
		} else if (serial[0] == 'F') {
			dolby_cp850(sources, prefix, serial_to_int(serial.substr(1)));
		} else {
			throw runtime_error("unrecognised Dolby / Doremi serial number format (does not start with a number, H or F)");
		}
	} else {
		throw runtime_error(fmt::format("unsupported manufacturer '{}' (supported: GDC, Barco, Christie, Qube, Dolby, Doremi)", maker));
	}

	return sources;
}


optional<string>
download_certificate(string maker, string serial, boost::filesystem::path file)
try
{
	string errors;
	for (auto const& source: certificate_sources(maker, serial)) {
		auto load = [file, source](boost::filesystem::path downloaded, string) -> optional<string> {
			try {
				/* Check that what we got really is a certificate before we keep it */
				auto const text = dcp::file_to_string(downloaded);
				auto const certificate = source.chain ? dcp::CertificateChain(text).leaf() : dcp::Certificate(text);
				dcp::write_string_to_file(certificate.certificate(true), file);
			} catch (dcp::MiscError& e) {
				return string(fmt::format("could not read the downloaded certificate ({})", e.what()));
			}
			return {};
		};

		auto error = source.zip_file
			? get_from_zip_url(source.url, *source.zip_file, true, true, load)
			: get_from_url(source.url, true, false, load);
		if (!error) {
			return {};
		}
		errors += *error + "\n";
	}
	boost::algorithm::trim(errors);
	return errors.empty() ? string("no certificate found") : errors;
}
catch (runtime_error& e)
{
	return string(e.what());
}
