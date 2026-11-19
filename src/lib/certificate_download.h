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


/** @file  src/lib/certificate_download.h
 *  @brief Download a projector / media block certificate from a manufacturer's server by serial number.
 */


#ifndef DCPOMATIC_CERTIFICATE_DOWNLOAD_H
#define DCPOMATIC_CERTIFICATE_DOWNLOAD_H


#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <list>
#include <string>
#include <vector>


#include "internet.h"


/** One place a device's certificate may be downloaded from */
class CertificateSource
{
public:
	CertificateSource(std::string url_, boost::optional<std::string> zip_file_ = {}, bool chain_ = false)
		: url(url_)
		, zip_file(zip_file_)
		, chain(chain_)
	{}

	std::string url;
	/** name of the file to take out of the ZIP file at \ref url, if it is a ZIP file */
	boost::optional<std::string> zip_file;
	/** true if the downloaded file is a certificate chain of which we want the leaf */
	bool chain = false;
};


/** The places from which the certificate of a manufacturer's device may be downloaded, in the order to try them
 *  (the same ones as the "Download certificate" dialog of the GUI uses).
 *  @param maker Manufacturer, in any case: GDC, Barco, Christie, Qube (tries QXI then QXPD; QXI and QXPD alone also work),
 *  Dolby or Doremi.
 *  @param serial Serial number of the device.
 *  @param ls Function to list the files of a directory URL; only Dolby / Doremi and Qube need it.
 *  GDC, Barco and Christie take their user name and password from the configuration.
 *  @throw std::runtime_error if the manufacturer is not supported, the serial number is not valid for it
 *  or the credentials are not in the configuration.
 */
std::vector<CertificateSource> certificate_sources(
	std::string maker,
	std::string serial,
	std::function<std::list<std::string> (std::string)> ls = ls_url
	);


/** Download the certificate of a device (trying each source in turn) and write it to a file.
 *  @param file File to write (PEM).
 *  @return error message, or empty on success.
 */
boost::optional<std::string> download_certificate(std::string maker, std::string serial, boost::filesystem::path file);


#endif
