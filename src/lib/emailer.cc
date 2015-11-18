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

#include "compose.hpp"
#include "job.h"
#include "data.h"
#include "config.h"
#include "emailer.h"
#include "exceptions.h"
#include <curl/curl.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::string;
using std::stringstream;
using std::min;
using std::list;
using std::cout;
using std::pair;
using boost::shared_ptr;

Emailer::Emailer (string from, list<string> to, string subject, string body)
	: _from (from)
	, _to (to)
	, _subject (subject)
	, _body (body)
	, _offset (0)
{
	boost::algorithm::replace_all (_body, "\n", "\r\n");
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
	memcpy (ptr, _email.substr(_offset, size * nmemb).c_str(), size * nmemb);
	_offset += t;
	return t;
}

void
Emailer::send (shared_ptr<Job> job)
{
	char date_buffer[32];
	time_t now = time (0);
	strftime (date_buffer, sizeof(date_buffer), "%a, %d %b %Y %H:%M:%S ", localtime (&now));

	boost::posix_time::ptime const utc_now = boost::posix_time::second_clock::universal_time ();
	boost::posix_time::ptime const local_now = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local (utc_now);
	boost::posix_time::time_duration offset = local_now - utc_now;
	sprintf (date_buffer + strlen(date_buffer), "%s%02d%02d", (offset.hours() >= 0 ? "+" : "-"), abs (offset.hours()), offset.minutes());

	stringstream email;

	email << "Date: " << date_buffer << "\r\n"
	      << "To: " << address_list (_to) << "\r\n"
	      << "From: " << _from << "\r\n";

	if (!_cc.empty ()) {
		email << "Cc: " << address_list (_cc);
	}

	if (!_bcc.empty ()) {
		email << "Bcc: " << address_list (_bcc);
	}

	string const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	string boundary;
	for (int i = 0; i < 32; ++i) {
		boundary += chars[rand() % chars.length()];
	}

	if (!_attachments.empty ()) {
		email << "MIME-Version: 1.0\r\n"
		      << "Content-Type: multipart/mixed; boundary=" << boundary << "\r\n";
	}

	email << "Subject: " << _subject << "\r\n"
	      << "User-Agent: DCP-o-matic\r\n"
	      << "\r\n";

	if (!_attachments.empty ()) {
		email << "--" << boundary << "\r\n"
		      << "Content-Type: text/plain; charset=utf-8\r\n\r\n";
	}

	email << _body;

	BOOST_FOREACH (Attachment i, _attachments) {
		email << "\r\n\r\n--" << boundary << "\r\n"
		      << "Content-Type: " << i.mime_type << "; name=" << i.name << "\r\n"
		      << "Content-Transfer-Encoding: Base64\r\n"
		      << "Content-Disposition: attachment; filename=" << i.name << "\r\n\r\n";

		BIO* b64 = BIO_new (BIO_f_base64());

		BIO* bio = BIO_new (BIO_s_mem());
		bio = BIO_push (b64, bio);

		Data data (i.file);
		BIO_write (bio, data.data().get(), data.size());
		(void) BIO_flush (bio);

		char* out;
		long int bytes = BIO_get_mem_data (bio, &out);
		email << string (out, bytes);

		BIO_free_all (b64);
	}

	if (!_attachments.empty ()) {
		email << "\r\n--" << boundary << "--\r\n";
	}

	_email = email.str ();

	curl_global_init (CURL_GLOBAL_DEFAULT);

	CURL* curl = curl_easy_init ();
	if (!curl) {
		throw NetworkError ("Could not initialise libcurl");
	}

	CURLM* mcurl = curl_multi_init ();
	if (!mcurl) {
		throw NetworkError ("Could not initialise libcurl");
	}

	curl_easy_setopt (curl, CURLOPT_URL, String::compose (
				  "smtp://%1:%2",
				  Config::instance()->mail_server().c_str(),
				  Config::instance()->mail_port()
				  ).c_str());

	if (!Config::instance()->mail_user().empty ()) {
		curl_easy_setopt (curl, CURLOPT_USERNAME, Config::instance()->mail_user().c_str());
	}
	if (!Config::instance()->mail_password().empty ()) {
		curl_easy_setopt (curl, CURLOPT_PASSWORD, Config::instance()->mail_password().c_str());
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

	curl_multi_add_handle (mcurl, curl);

	time_t start = time (0);

	int still_running = 1;
	curl_multi_perform (mcurl, &still_running);

	while (still_running) {

		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;

		FD_ZERO (&fdread);
		FD_ZERO (&fdwrite);
		FD_ZERO (&fdexcep);

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		long curl_timeout = -1;
		curl_multi_timeout (mcurl, &curl_timeout);
		if (curl_timeout >= 0) {
			timeout.tv_sec = curl_timeout / 1000;
			if (timeout.tv_sec > 1) {
				timeout.tv_sec = 1;
			} else {
				timeout.tv_usec = (curl_timeout % 1000) * 1000;
			}
		}

		int maxfd = -1;
		CURLMcode mc = curl_multi_fdset (mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);

		if (mc != CURLM_OK) {
			throw KDMError (String::compose ("Failed to send KDM email to %1", address_list (_to)));
		}

		int rc;
		if (maxfd == -1) {
#ifdef DCPOMATIC_WINDOWS
			Sleep (100);
			rc = 0;
#else
			struct timeval wait = { 0, 100 * 1000};
			rc = select (0, 0, 0, 0, &wait);
#endif
		} else {
			rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		if (rc < 0) {
			throw KDMError ("Failed to send KDM email");
		}

		mc = curl_multi_perform (mcurl, &still_running);
		if (mc != CURLM_OK) {
			throw KDMError (String::compose ("Failed to send KDM email (%1)", curl_multi_strerror (mc)));
		}

		if (job) {
			job->set_progress_unknown ();
		}

		if ((time(0) - start) > 10) {
			throw KDMError (_("Failed to send KDM email (timed out)"));
		}
	}

	int messages;
	do {
		CURLMsg* m = curl_multi_info_read (mcurl, &messages);
		if (m && m->data.result != CURLE_OK) {
			throw KDMError (String::compose ("Failed to send KDM email (%1)", curl_easy_strerror (m->data.result)));
		}
	} while (messages > 0);

	/* XXX: we should do this stuff when an exception is thrown, but curl_multi_remove_handle
	   seems to hang if we try that.
	*/

	curl_slist_free_all (recipients);
	curl_multi_remove_handle (mcurl, curl);
	curl_multi_cleanup (mcurl);
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
