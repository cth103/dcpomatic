/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "curl_uploader.h"
#include "exceptions.h"
#include "config.h"
#include "cross.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::function;

static size_t
read_callback (void* ptr, size_t size, size_t nmemb, void* object)
{
	CurlUploader* u = reinterpret_cast<CurlUploader*> (object);
	return u->read_callback (ptr, size, nmemb);
}

CurlUploader::CurlUploader (function<void (string)> set_status, function<void (float)> set_progress)
	: Uploader (set_status, set_progress)
	, _file (0)
	, _transferred (0)
	, _total_size (0)
{
	_curl = curl_easy_init ();
	if (!_curl) {
		throw NetworkError (_("Could not start transfer"));
	}

	curl_easy_setopt (_curl, CURLOPT_READFUNCTION, ::read_callback);
	curl_easy_setopt (_curl, CURLOPT_READDATA, this);
	curl_easy_setopt (_curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt (_curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
	curl_easy_setopt (_curl, CURLOPT_READDATA, this);
	curl_easy_setopt (_curl, CURLOPT_USERNAME, Config::instance()->tms_user().c_str ());
	curl_easy_setopt (_curl, CURLOPT_PASSWORD, Config::instance()->tms_password().c_str ());
}

CurlUploader::~CurlUploader ()
{
	if (_file) {
		fclose (_file);
	}
	curl_easy_cleanup (_curl);
}

void
CurlUploader::create_directory (boost::filesystem::path)
{
	/* this is done by libcurl */
}

void
CurlUploader::upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size)
{
	curl_easy_setopt (
		_curl, CURLOPT_URL,
		/* Use generic_string so that we get forward-slashes in the path, even on Windows */
		String::compose ("ftp://%1/%2/%3", Config::instance()->tms_ip(), Config::instance()->tms_path(), to.generic_string ()).c_str ()
		);

	_file = fopen_boost (from, "rb");
	if (!_file) {
		throw NetworkError (String::compose (_("Could not open %1 to send"), from));
	}
	_transferred = &transferred;
	_total_size = total_size;

	CURLcode const r = curl_easy_perform (_curl);
	if (r != CURLE_OK) {
		throw NetworkError (String::compose (_("Could not write to remote file (%1)"), curl_easy_strerror (r)));
	}

	fclose (_file);
	_file = 0;
}

size_t
CurlUploader::read_callback (void* ptr, size_t size, size_t nmemb)
{
	size_t const r = fread (ptr, size, nmemb, _file);
	*_transferred += size * nmemb;

	if (_total_size > 0) {
		_set_progress ((double) *_transferred / _total_size);
	}

	return r;
}
