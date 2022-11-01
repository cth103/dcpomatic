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


#include "uploader.h"
#include <dcp/file.h>
#include <curl/curl.h>


class CurlUploader : public Uploader
{
public:
	CurlUploader (std::function<void (std::string)> set_status, std::function<void (float)> set_progress);
	~CurlUploader ();

	size_t read_callback (void* ptr, size_t size, size_t nmemb);
	int debug(CURL* curl, curl_infotype type, char* data, size_t size);

protected:
	void create_directory (boost::filesystem::path directory) override;
	void upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size) override;

private:
	CURL* _curl;

	FILE* _file = nullptr;
	boost::uintmax_t* _transferred = nullptr;
	boost::uintmax_t _total_size = 0;
};
