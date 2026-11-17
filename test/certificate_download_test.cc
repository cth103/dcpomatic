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

#include "lib/certificate_download.h"
#include "lib/config.h"
#include "lib/kdm_cli.h"
#include <list>
#include <boost/test/unit_test.hpp>


using std::runtime_error;
using std::string;
using std::vector;


static
vector<string>
urls(string maker, string serial, std::function<std::list<string> (string)> ls = [](string) { return std::list<string>(); })
{
	vector<string> result;
	for (auto const& source: certificate_sources(maker, serial, ls)) {
		result.push_back(source.url + (source.zip_file ? " [" + *source.zip_file + "]" : "") + (source.chain ? " (chain)" : ""));
	}
	return result;
}


BOOST_AUTO_TEST_CASE(certificate_sources_gdc_test)
{
	auto config = Config::instance();
	config->set_gdc_username("fred");
	config->set_gdc_password("secret");
	BOOST_CHECK(urls("GDC", "A123456") == vector<string>({"ftp://fred:secret@ftp.gdc-tech.com/SHA256/A123456.crt.pem"}));
	BOOST_CHECK(urls("gdc", " A123456 ") == vector<string>({"ftp://fred:secret@ftp.gdc-tech.com/SHA256/A123456.crt.pem"}));
	config->unset_gdc_username();
	config->unset_gdc_password();
}


BOOST_AUTO_TEST_CASE(certificate_sources_barco_test)
{
	auto config = Config::instance();
	config->set_barco_username("fred");
	config->set_barco_password("secret");
	BOOST_CHECK(
		urls("Barco", "1234567890") ==
		vector<string>({"sftp://fred:secret@certificates.barco.com/1234567xxx/1234567890/Barco-ICMP.1234567890_cert.pem"})
		);
	BOOST_CHECK_THROW(urls("Barco", "12345"), runtime_error);
	config->unset_barco_username();
	config->unset_barco_password();
}


BOOST_AUTO_TEST_CASE(certificate_sources_christie_test)
{
	auto config = Config::instance();
	config->set_christie_username("fred");
	config->set_christie_password("secret");
	BOOST_CHECK(
		urls("Christie", "1234") == vector<string>({
			"ftp://fred:secret@certificates.christiedigital.com/Certificates/F-IMB/F-IMB_000000001234_sha256.pem (chain)",
			"ftp://fred:secret@certificates.christiedigital.com/Certificates/IMB-S2/IMB-S2_000000001234_sha256.pem (chain)"
			})
		);
	config->unset_christie_username();
	config->unset_christie_password();
}


BOOST_AUTO_TEST_CASE(certificate_sources_qube_test)
{
	auto ls = [](string directory) {
		if (directory == "ftp://certificates.qubecinema.com/SMPTE-QXI/") {
			return std::list<string>({"QXI-12345-abc.pem", "QXI-99999-def.pem"});
		}
		return std::list<string>({"QXPD-777-xyz.pem"});
	};
	BOOST_CHECK(urls("Qube", "12345", ls) == vector<string>({"ftp://certificates.qubecinema.com/SMPTE-QXI/QXI-12345-abc.pem"}));
	BOOST_CHECK(urls("Qube", "777", ls) == vector<string>({"ftp://certificates.qubecinema.com/SMPTE-QXPD/QXPD-777-xyz.pem"}));
	BOOST_CHECK(urls("qxpd", "777", ls) == vector<string>({"ftp://certificates.qubecinema.com/SMPTE-QXPD/QXPD-777-xyz.pem"}));
	BOOST_CHECK_THROW(urls("qxpd", "12345", ls), runtime_error);
}


BOOST_AUTO_TEST_CASE(certificate_sources_dolby_test)
{
	string const p = "ftp://ftp.cinema.dolby.com/Certificates/";
	auto ls = [](string) { return std::list<string>({"Dolby-IMS3000-100123.dcicerts.zip", "cert_Dolby-IMS3000-100123-SMPTE.zip"}); };
	BOOST_CHECK(urls("Dolby", "100123", ls) == vector<string>({
		p + "100xxx/cert_Dolby-IMS3000-100123-SMPTE.zip [cert_Dolby-IMS3000-100123-SMPTE.pem]",
		p + "CAT862_510999_and_lower/cert_Dolby256-CAT862-100123.zip [cert_Dolby256-CAT862-100123.pem.crt]",
		p + "DSP100_3000_and_higher/cert_Dolby256-DSP100-100123.zip [cert_Dolby256-DSP100-100123.pem.crt]"
		}));
	BOOST_CHECK(urls("Doremi", "H1500") == vector<string>({
		p + "CAT745_1000_thru_1999/cert_Dolby-CAT745-1500.zip [cert_Dolby-CAT745-1500.pem.crt]"
		}));
	BOOST_CHECK(urls("Dolby", "F12345") == vector<string>({
		p + "CP850_CAT1600_F12000-F12999/cert_RMB_SPB_MDE_FMA.Dolby-CP850-F12345.zip [cert_RMB_SPB_MDE_FMA.Dolby-CP850-F12345.pem.crt]"
		}));
	BOOST_CHECK_THROW(urls("Dolby", "X1"), runtime_error);
}


BOOST_AUTO_TEST_CASE(certificate_sources_errors_test)
{
	auto config = Config::instance();
	config->unset_gdc_username();
	config->unset_gdc_password();
	/* No credentials */
	BOOST_CHECK_THROW(urls("GDC", "A123456"), runtime_error);
	/* Unknown manufacturer */
	BOOST_CHECK_THROW(urls("Acme", "A123456"), runtime_error);
	/* A serial number cannot be used to change the URL */
	config->set_gdc_username("fred");
	config->set_gdc_password("secret");
	BOOST_CHECK_THROW(urls("GDC", "../A123456"), runtime_error);
	BOOST_CHECK_THROW(urls("GDC", "A1@evil.com"), runtime_error);
	BOOST_CHECK_THROW(urls("GDC", ""), runtime_error);
	config->unset_gdc_username();
	config->unset_gdc_password();
}


BOOST_AUTO_TEST_CASE(kdm_cli_download_certificate_errors_test)
{
	auto run = [](vector<string> const& args) {
		std::vector<char*> argv(args.size());
		for (auto i = 0U; i < args.size(); ++i) {
			argv[i] = const_cast<char*>(args[i].c_str());
		}
		return kdm_cli(args.size(), argv.data(), [](string) {});
	};

	Config::instance()->unset_gdc_username();
	Config::instance()->unset_gdc_password();
	/* Missing arguments */
	BOOST_REQUIRE(run({"kdm_cli", "download-certificate"}));
	BOOST_REQUIRE(run({"kdm_cli", "download-certificate", "GDC"}));
	/* Missing credentials: reported without trying the network */
	auto error = run({"kdm_cli", "download-certificate", "GDC", "A99113"});
	BOOST_REQUIRE(error);
	BOOST_CHECK(error->find("user name and password") != string::npos);
}
