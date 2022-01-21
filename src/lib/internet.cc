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


#include "scoped_temporary.h"
#include "compose.hpp"
#include "exceptions.h"
#include "cross.h"
#include "util.h"
#include <curl/curl.h>
#include <zip.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <string>

#include "i18n.h"


using std::function;
using std::list;
using std::string;
using boost::optional;
using boost::algorithm::trim;


static size_t
ls_url_data (void* buffer, size_t size, size_t nmemb, void* output)
{
	string* s = reinterpret_cast<string*>(output);
	char* c = reinterpret_cast<char*>(buffer);
	for (size_t i = 0; i < (size * nmemb); ++i) {
		*s += c[i];
	}
	return nmemb;
}


list<string>
ls_url (string url)
{
	auto curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt (curl, CURLOPT_DIRLISTONLY, 1);

	string ls;
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ls_url_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ls);
	auto const cr = curl_easy_perform (curl);

	if (cr != CURLE_OK) {
		return list<string>();
	}

	list<string> result;
	result.push_back("");
	for (size_t i = 0; i < ls.size(); ++i) {
		if (ls[i] == '\n') {
			result.push_back("");
		} else {
			result.back() += ls[i];
		}
	}

	result.pop_back ();
	return result;
}


static size_t
get_from_url_data (void* buffer, size_t size, size_t nmemb, void* stream)
{
	auto f = reinterpret_cast<FILE*> (stream);
	return fwrite (buffer, size, nmemb, f);
}


optional<string>
get_from_url (string url, bool pasv, bool skip_pasv_ip, ScopedTemporary& temp)
{
	auto curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str());

	auto f = temp.open ("wb");
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, get_from_url_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPSV, 0);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPRT, 0);
	if (skip_pasv_ip) {
		curl_easy_setopt (curl, CURLOPT_FTP_SKIP_PASV_IP, 1);
	}
	if (!pasv) {
		curl_easy_setopt (curl, CURLOPT_FTPPORT, "-");
	}

	/* Maximum time is 20s */
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, 20);

	auto const cr = curl_easy_perform (curl);

	temp.close ();
	curl_easy_cleanup (curl);
	if (cr != CURLE_OK) {
		return String::compose (_("Download failed (%1 error %2)"), url, (int) cr);
	}

	return {};
}


optional<string>
get_from_url (string url, bool pasv, bool skip_pasv_ip, function<optional<string> (boost::filesystem::path)> load)
{
	ScopedTemporary temp;
	auto e = get_from_url (url, pasv, skip_pasv_ip, temp);
	if (e) {
		return e;
	}
	return load (temp.file());
}


/** @param url URL of ZIP file.
 *  @param file Filename within ZIP file.
 *  @param load Function passed a (temporary) filesystem path of the unpacked file.
 */
optional<string>
get_from_zip_url (string url, string file, bool pasv, bool skip_pasv_ip, function<optional<string> (boost::filesystem::path)> load)
{
	/* Download the ZIP file to temp_zip */
	ScopedTemporary temp_zip;
	auto e = get_from_url (url, pasv, skip_pasv_ip, temp_zip);
	if (e) {
		return e;
	}

	/* Open the ZIP file and read `file' out of it */

#ifdef DCPOMATIC_HAVE_ZIP_SOURCE_T
	/* This is the way to do it with newer versions of libzip, and is required on Windows.
	   The zip_source_t API is missing in the libzip versions shipped with Ubuntu 14.04,
	   Centos 6, Centos 7, Debian 7 and Debian 8.
	*/

	auto zip_file = fopen_boost (temp_zip.file (), "rb");
	if (!zip_file) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}

	auto zip_source = zip_source_filep_create (zip_file, 0, -1, 0);
	if (!zip_source) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}

	zip_error_t error;
	zip_error_init (&error);
	auto zip = zip_open_from_source (zip_source, ZIP_RDONLY, &error);
	if (!zip) {
		return String::compose (_("Could not open downloaded ZIP file (%1:%2: %3)"), error.zip_err, error.sys_err, error.str ? error.str : "");
	}

#else
	struct zip* zip = zip_open (temp_zip.c_str(), 0, 0);
#endif

	struct zip_file* file_in_zip = zip_fopen (zip, file.c_str(), 0);
	if (!file_in_zip) {
		return optional<string> (_("Unexpected ZIP file contents"));
	}

	ScopedTemporary temp_cert;
	auto f = temp_cert.open ("wb");
	char buffer[4096];
	while (true) {
		int const N = zip_fread (file_in_zip, buffer, sizeof (buffer));
		checked_fwrite (buffer, N, f, temp_cert.file());
		if (N < int (sizeof (buffer))) {
			break;
		}
	}
	zip_fclose (file_in_zip);
	zip_close (zip);
	temp_cert.close ();

	return load (temp_cert.file());
}
