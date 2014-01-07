/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include <sstream>
#include <curl/curl.h>
#include <libcxml/cxml.h>
#include "update.h"
#include "version.h"

#define BUFFER_SIZE 1024

using std::cout;
using std::min;
using std::string;
using std::stringstream;

UpdateChecker::UpdateChecker ()
	: _buffer (new char[BUFFER_SIZE])
	, _offset (0)
{
	
}

UpdateChecker::~UpdateChecker ()
{
	delete[] _buffer;
}

static size_t
write_callback_wrapper (void* data, size_t size, size_t nmemb, void* user)
{
	return reinterpret_cast<UpdateChecker*>(user)->write_callback (data, size, nmemb);
}

UpdateChecker::Result
UpdateChecker::run ()
{
	curl_global_init (CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init ();
	if (!curl) {
		return MAYBE;
	}

	curl_easy_setopt (curl, CURLOPT_URL, "http://dcpomatic.com/update.php");
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_callback_wrapper);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, this);
	string const agent = "dcpomatic/" + string (dcpomatic_version);
	curl_easy_setopt (curl, CURLOPT_USERAGENT, agent.c_str ());
	int r = curl_easy_perform (curl);
	if (r != CURLE_OK) {
		return MAYBE;
	}

	_buffer[BUFFER_SIZE-1] = '\0';
	stringstream s;
	s << _buffer;
	cxml::Document doc ("Update");
	doc.read_stream (s);

	cout << doc.string_child ("Stable") << "\n";
	return YES;
}

size_t
UpdateChecker::write_callback (void* data, size_t size, size_t nmemb)
{
	size_t const t = min (size * nmemb, size_t (BUFFER_SIZE - _offset));
	memcpy (_buffer + _offset, data, t);
	_offset += t;
	return t;
}
