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

#include <curl/curl.h>
#include <boost/scoped_array.hpp>

class Emailer
{
public:
	Emailer (std::string from, std::list<std::string> to, std::string subject, std::string body);

	void add_cc (std::string cc);
	void add_bcc (std::string bcc);
	void add_attachment (boost::filesystem::path file, std::string name, std::string mime_type);

	void send ();

	std::string notes () const {
		return _notes;
	}

	size_t get_data (void* ptr, size_t size, size_t nmemb);
	int debug (CURL* curl, curl_infotype type, char* data, size_t size);

	static std::string address_list (std::list<std::string> addresses);

private:

	std::string _from;
	std::list<std::string> _to;
	std::string _subject;
	std::string _body;
	std::list<std::string> _cc;
	std::list<std::string> _bcc;

	struct Attachment {
		boost::filesystem::path file;
		std::string name;
		std::string mime_type;
	};

	std::list<Attachment> _attachments;
	std::string _email;
	size_t _offset;
	std::string _notes;
};
