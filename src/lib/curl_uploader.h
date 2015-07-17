/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "uploader.h"
#include <curl/curl.h>

class CurlUploader : public Uploader
{
public:
	CurlUploader (boost::function<void (std::string)> set_status, boost::function<void (float)> set_progress);
	~CurlUploader ();

	size_t read_callback (void* ptr, size_t size, size_t nmemb);

protected:
	virtual void create_directory (boost::filesystem::path directory);
	virtual void upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size);

private:
	CURL* _curl;

	FILE* _file;
	boost::uintmax_t* _transferred;
	boost::uintmax_t _total_size;
};
