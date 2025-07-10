/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "config.h"
#include "cross.h"
#include "dcpomatic_assert.h"
#include <iostream>

#include "i18n.h"


using std::string;
using std::cout;
using std::function;


static size_t
read_callback(void* ptr, size_t size, size_t nmemb, void* object)
{
	CurlUploader* u = reinterpret_cast<CurlUploader*>(object);
	return u->read_callback(ptr, size, nmemb);
}


static int
curl_debug_shim(CURL* curl, curl_infotype type, char* data, size_t size, void* userp)
{
	return reinterpret_cast<CurlUploader*>(userp)->debug(curl, type, data, size);
}


CurlUploader::CurlUploader(function<void (string)> set_status, function<void (float)> set_progress)
	: Uploader(set_status, set_progress)
{
	_curl = curl_easy_init();
	if (!_curl) {
		throw NetworkError(_("Could not start transfer"));
	}

	curl_easy_setopt(_curl, CURLOPT_READFUNCTION, ::read_callback);
	curl_easy_setopt(_curl, CURLOPT_READDATA, this);
	curl_easy_setopt(_curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(_curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
	curl_easy_setopt(_curl, CURLOPT_READDATA, this);
	curl_easy_setopt(_curl, CURLOPT_USERNAME, Config::instance()->tms_user().c_str());
	curl_easy_setopt(_curl, CURLOPT_PASSWORD, Config::instance()->tms_password().c_str());
	if (!Config::instance()->tms_passive()) {
		curl_easy_setopt(_curl, CURLOPT_FTPPORT, "-");
	}
	curl_easy_setopt(_curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(_curl, CURLOPT_DEBUGFUNCTION, curl_debug_shim);
	curl_easy_setopt(_curl, CURLOPT_DEBUGDATA, this);
}


CurlUploader::~CurlUploader()
{
	curl_easy_cleanup(_curl);
}


void
CurlUploader::create_directory(boost::filesystem::path)
{
	/* this is done by libcurl */
}


void
CurlUploader::upload_file(boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size)
{
	curl_easy_setopt(
		_curl, CURLOPT_URL,
		/* Use generic_string so that we get forward-slashes in the path, even on Windows */
		fmt::format("ftp://{}/{}/{}", Config::instance()->tms_ip(), Config::instance()->tms_path(), to.generic_string()).c_str()
		);

	dcp::File file(from, "rb");
	if (!file) {
		throw NetworkError(fmt::format(_("Could not open {} to send"), from.string()));
	}
	_file = file.get();
	_transferred = &transferred;
	_total_size = total_size;

	auto const r = curl_easy_perform(_curl);
	if (r != CURLE_OK) {
		throw NetworkError(fmt::format(_("Could not write to remote file ({})"), curl_easy_strerror(r)));
	}

	_file = nullptr;
}


size_t
CurlUploader::read_callback(void* ptr, size_t size, size_t nmemb)
{
	DCPOMATIC_ASSERT(_file);
	size_t const r = fread(ptr, size, nmemb, _file);
	*_transferred += size * nmemb;

	if (_total_size > 0) {
		_set_progress((double) *_transferred / _total_size);
	}

	return r;
}


int
CurlUploader::debug(CURL *, curl_infotype type, char* data, size_t size)
{
	if (type == CURLINFO_TEXT && size > 0) {
		LOG_GENERAL("CurlUploader: {}", string(data, size - 1));
	}
	return 0;
}

