/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include <string>
#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include <zip.h>
#include "scoped_temporary.h"
#include "compose.hpp"
#include "safe_stringstream.h"

#include "i18n.h"

using std::string;
using std::list;
using boost::optional;
using boost::function;
using boost::algorithm::trim;

static size_t
get_from_zip_url_data (void* buffer, size_t size, size_t nmemb, void* stream)
{
	FILE* f = reinterpret_cast<FILE*> (stream);
	return fwrite (buffer, size, nmemb, f);
}

/** @param url URL of ZIP file.
 *  @param file Filename within ZIP file.
 *  @param load Function passed a (temporary) filesystem path of the unpacked file.
 */
optional<string>
get_from_zip_url (string url, string file, function<void (boost::filesystem::path)> load)
{
	/* Download the ZIP file to temp_zip */
	CURL* curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
	
	ScopedTemporary temp_zip;
	FILE* f = temp_zip.open ("wb");
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, get_from_zip_url_data);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPSV, 0);
	/* Maximum time is 20s */
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, 20);

	CURLcode const cr = curl_easy_perform (curl);

	temp_zip.close ();
	curl_easy_cleanup (curl);
	if (cr != CURLE_OK) {
		return String::compose (_("Download failed (%1/%2 error %3)"), url, file, cr);
	}

	/* Open the ZIP file and read `file' out of it */
	
	struct zip* zip = zip_open (temp_zip.c_str(), 0, 0);
	if (!zip) {
		return optional<string> (_("Could not open downloaded ZIP file"));
	}
	
	struct zip_file* zip_file = zip_fopen (zip, file.c_str(), 0);
	if (!zip_file) {
		return optional<string> (_("Unexpected ZIP file contents"));
	}
	
	ScopedTemporary temp_cert;
	f = temp_cert.open ("wb");
	char buffer[4096];
	while (true) {
		int const N = zip_fread (zip_file, buffer, sizeof (buffer));
		fwrite (buffer, 1, N, f);
		if (N < int (sizeof (buffer))) {
			break;
		}
	}
	temp_cert.close ();
	
	load (temp_cert.file ());
	return optional<string> ();
}


static size_t
ftp_ls_data (void* buffer, size_t size, size_t nmemb, void* data)
{
	string* s = reinterpret_cast<string *> (data);
	uint8_t* b = reinterpret_cast<uint8_t *> (buffer);
	for (size_t i = 0; i < (size * nmemb); ++i) {
		*s += b[i];
	}
	return nmemb;
}

list<string>
ftp_ls (string url)
{
	CURL* curl = curl_easy_init ();
	if (!curl) {
		return list<string> ();
	}

	if (url.substr (url.length() - 1, 1) != "/") {
		url += "/";
	}
	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
	/* 20s timeout */
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, 20);

	string ls_raw;
	struct curl_slist* commands = 0;
	commands = curl_slist_append (commands, "NLST");
	curl_easy_setopt (curl, CURLOPT_POSTQUOTE, commands);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ls_raw);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ftp_ls_data);
	curl_easy_setopt (curl, CURLOPT_FTP_USE_EPSV, 0);
	CURLcode const r = curl_easy_perform (curl);
	if (r != CURLE_OK) {
		return list<string> ();
	}

	SafeStringStream s (ls_raw);
	list<string> ls;
	while (s.good ()) {
		string line = s.getline ();
		trim (line);
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
