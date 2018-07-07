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

#include "compose.hpp"
#include "config.h"
#include "emailer.h"
#include "exceptions.h"
#include <curl/curl.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::string;
using std::min;
using std::list;
using std::cout;
using std::pair;
using boost::shared_ptr;
using dcp::Data;

Emailer::Emailer (string from, list<string> to, string subject, string body)
	: _from (from)
	, _to (to)
	, _subject (subject)
	, _body (fix (body))
	, _offset (0)
{

}

string
Emailer::fix (string s) const
{
	boost::algorithm::replace_all (s, "\n", "\r\n");
	boost::algorithm::replace_all (s, "\0", " ");
	return s;
}

void
Emailer::add_cc (string cc)
{
	_cc.push_back (cc);
}

void
Emailer::add_bcc (string bcc)
{
	_bcc.push_back (bcc);
}

void
Emailer::add_attachment (boost::filesystem::path file, string name, string mime_type)
{
	Attachment a;
	a.file = file;
	a.name = name;
	a.mime_type = mime_type;
	_attachments.push_back (a);
}

static size_t
curl_data_shim (void* ptr, size_t size, size_t nmemb, void* userp)
{
	return reinterpret_cast<Emailer*>(userp)->get_data (ptr, size, nmemb);
}

static int
curl_debug_shim (CURL* curl, curl_infotype type, char* data, size_t size, void* userp)
{
	return reinterpret_cast<Emailer*>(userp)->debug (curl, type, data, size);
}

size_t
Emailer::get_data (void* ptr, size_t size, size_t nmemb)
{
	size_t const t = min (_email.length() - _offset, size * nmemb);
	memcpy (ptr, _email.substr (_offset, t).c_str(), t);
	_offset += t;
	return t;
}

void
Emailer::send (string server, int port, string user, string password)
{
	char date_buffer[128];
	time_t now = time (0);
	strftime (date_buffer, sizeof(date_buffer), "%a, %d %b %Y %H:%M:%S ", localtime (&now));

	boost::posix_time::ptime const utc_now = boost::posix_time::second_clock::universal_time ();
	boost::posix_time::ptime const local_now = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local (utc_now);
	boost::posix_time::time_duration offset = local_now - utc_now;
	sprintf (date_buffer + strlen(date_buffer), "%s%02d%02d", (offset.hours() >= 0 ? "+" : "-"), int(abs(offset.hours())), int(offset.minutes()));

	_email = "Date: " + string(date_buffer) + "\r\n"
		"To: " + address_list (_to) + "\r\n"
		"From: " + _from + "\r\n";

	if (!_cc.empty ()) {
		_email += "Cc: " + address_list (_cc) + "\r\n";
	}

	if (!_bcc.empty ()) {
		_email += "Bcc: " + address_list (_bcc) + "\r\n";
	}

	string const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	string boundary;
	for (int i = 0; i < 32; ++i) {
		boundary += chars[rand() % chars.length()];
	}

	if (!_attachments.empty ()) {
		_email += "MIME-Version: 1.0\r\n"
			"Content-Type: multipart/mixed; boundary=" + boundary + "\r\n";
	}

	_email += "Subject: " + _subject + "\r\n"
		"User-Agent: DCP-o-matic\r\n"
		"\r\n";

	if (!_attachments.empty ()) {
		_email += "--" + boundary + "\r\n"
			+ "Content-Type: text/plain; charset=utf-8\r\n\r\n";
	}

	_email += _body;

	BOOST_FOREACH (Attachment i, _attachments) {
		_email += "\r\n\r\n--" + boundary + "\r\n"
			"Content-Type: " + i.mime_type + "; name=" + i.name + "\r\n"
			"Content-Transfer-Encoding: Base64\r\n"
			"Content-Disposition: attachment; filename=" + i.name + "\r\n\r\n";

		BIO* b64 = BIO_new (BIO_f_base64());

		BIO* bio = BIO_new (BIO_s_mem());
		bio = BIO_push (b64, bio);

		Data data (i.file);
		BIO_write (bio, data.data().get(), data.size());
		(void) BIO_flush (bio);

		char* out;
		long int bytes = BIO_get_mem_data (bio, &out);
		_email += fix (string (out, bytes));

		BIO_free_all (b64);
	}

	if (!_attachments.empty ()) {
		_email += "\r\n--" + boundary + "--\r\n";
	}

	curl_global_init (CURL_GLOBAL_DEFAULT);

	CURL* curl = curl_easy_init ();
	if (!curl) {
		throw NetworkError ("Could not initialise libcurl");
	}

	if (port == 465) {
		/* "Implicit TLS"; I think curl wants us to use smtps here */
		curl_easy_setopt (curl, CURLOPT_URL, String::compose ("smtps://%1:465", server).c_str());
	} else {
		curl_easy_setopt (curl, CURLOPT_URL, String::compose ("smtp://%1:%2", server, port).c_str());
	}

	if (!user.empty ()) {
		curl_easy_setopt (curl, CURLOPT_USERNAME, user.c_str ());
	}
	if (!password.empty ()) {
		curl_easy_setopt (curl, CURLOPT_PASSWORD, password.c_str());
	}

	curl_easy_setopt (curl, CURLOPT_MAIL_FROM, _from.c_str());

	struct curl_slist* recipients = 0;
	BOOST_FOREACH (string i, _to) {
		recipients = curl_slist_append (recipients, i.c_str());
	}
	BOOST_FOREACH (string i, _cc) {
		recipients = curl_slist_append (recipients, i.c_str());
	}
	BOOST_FOREACH (string i, _bcc) {
		recipients = curl_slist_append (recipients, i.c_str());
	}

	curl_easy_setopt (curl, CURLOPT_MAIL_RCPT, recipients);

	curl_easy_setopt (curl, CURLOPT_READFUNCTION, curl_data_shim);
	curl_easy_setopt (curl, CURLOPT_READDATA, this);
	curl_easy_setopt (curl, CURLOPT_UPLOAD, 1L);

	curl_easy_setopt (curl, CURLOPT_USE_SSL, (long) CURLUSESSL_TRY);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt (curl, CURLOPT_DEBUGFUNCTION, curl_debug_shim);
	curl_easy_setopt (curl, CURLOPT_DEBUGDATA, this);

	CURLcode const r = curl_easy_perform (curl);
	if (r != CURLE_OK) {
		throw KDMError (_("Failed to send email"), curl_easy_strerror (r));
	}

	curl_slist_free_all (recipients);
	curl_easy_cleanup (curl);
	curl_global_cleanup ();
}

string
Emailer::address_list (list<string> addresses)
{
	string o;
	BOOST_FOREACH (string i, addresses) {
		o += i + ", ";
	}

	return o.substr (0, o.length() - 2);
}

int
Emailer::debug (CURL *, curl_infotype type, char* data, size_t size)
{
	if (type == CURLINFO_TEXT) {
		_notes += string (data, size);
	} else if (type == CURLINFO_HEADER_IN) {
		_notes += "<- " + string (data, size);
	} else if (type == CURLINFO_HEADER_OUT) {
		_notes += "-> " + string (data, size);
	}
	return 0;
}
