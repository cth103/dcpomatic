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

#include "scoped_temporary.h"
#include "compose.hpp"
#include "exceptions.h"
#include "cross.h"
#include <curl/curl.h>
#include <zip.h>
#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <string>

#include "i18n.h"

using std::string;
using std::list;
using boost::optional;
using boost::function;
using boost::algorithm::trim;


static size_t
get_from_url_data (void* buffer, size_t size, size_t nmemb, void* stream)
{
	FILE* f = reinterpret_cast<FILE*> (stream);
	return fwrite (buffer, size, nmemb, f);
}

static
optional<string>
get_from_url (string url, bool pasv, ScopedTemporary& temp)
{
	CURL* curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str());

	FILE* f = temp.open ("w");
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, get_from_url_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPSV, 0);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPRT, 0);
	if (!pasv) {
		curl_easy_setopt (curl, CURLOPT_FTPPORT, "-");
	}

	/* Maximum time is 20s */
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, 20);

	CURLcode const cr = curl_easy_perform (curl);

	temp.close ();
	curl_easy_cleanup (curl);
	if (cr != CURLE_OK) {
		return String::compose (_("Download failed (%1 error %2)"), url, (int) cr);
	}

	return optional<string>();
}

optional<string>
get_from_url (string url, bool pasv, function<void (boost::filesystem::path)> load)
{
	ScopedTemporary temp;
	optional<string> e = get_from_url (url, pasv, temp);
	if (e) {
		return e;
	}
	load (temp.file());
	return optional<string>();
}

/** @param url URL of ZIP file.
 *  @param file Filename within ZIP file.
 *  @param load Function passed a (temporary) filesystem path of the unpacked file.
 */
optional<string>
get_from_zip_url (string url, string file, bool pasv, function<void (boost::filesystem::path)> load)
{
	/* Download the ZIP file to temp_zip */
	ScopedTemporary temp_zip;
	optional<string> e = get_from_url (url, pasv, temp_zip);
	if (e) {
		return e;
	}

	/* Open the ZIP file and read `file' out of it */

#ifdef DCPOMATIC_HAVE_ZIP_SOURCE_T
	/* This is the way to do it with newer versions of libzip, and is required on Windows.
	   The zip_source_t API is missing in the libzip versions shipped with Ubuntu 14.04,
	   Centos 6, Centos 7, Debian 7 and Debian 8.
	*/

	FILE* zip_file = fopen_boost (temp_zip.file (), "rb");
	if (!zip_file) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}

	zip_source_t* zip_source = zip_source_filep_create (zip_file, 0, -1, 0);
	if (!zip_source) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}

	zip_t* zip = zip_open_from_source (zip_source, 0, 0);
	if (!zip) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}

#else
	struct zip* zip = zip_open (temp_zip.c_str(), 0, 0);
#endif

	struct zip_file* file_in_zip = zip_fopen (zip, file.c_str(), 0);
	if (!file_in_zip) {
		return optional<string> (_("Unexpected ZIP file contents"));
	}

	ScopedTemporary temp_cert;
	FILE* f = temp_cert.open ("wb");
	char buffer[4096];
	while (true) {
		int const N = zip_fread (file_in_zip, buffer, sizeof (buffer));
		fwrite (buffer, 1, N, f);
		if (N < int (sizeof (buffer))) {
			break;
		}
	}
	zip_fclose (file_in_zip);
	zip_close (zip);
	temp_cert.close ();

	load (temp_cert.file ());
	return optional<string> ();
}
