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


#include "config.h"
#include "dcpomatic_log.h"
#include "email.h"
#include "exceptions.h"
#include "util.h"
#include "variant.h"
#include <curl/curl.h>
#include <boost/algorithm/string.hpp>

#include "i18n.h"


using std::cout;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using dcp::ArrayData;


Email::Email(string from, vector<string> to, string subject, string body)
	: _from(from)
	, _to(to)
	, _subject(subject)
	, _body(fix(body))
	, _offset(0)
{

}


string
Email::fix(string s) const
{
	boost::algorithm::replace_all(s, "\n", "\r\n");
	boost::algorithm::replace_all(s, "\0", " ");
	return s;
}


void
Email::add_cc(string cc)
{
	_cc.push_back(cc);
}


void
Email::add_bcc(string bcc)
{
	_bcc.push_back(bcc);
}


void
Email::add_attachment(boost::filesystem::path file, string name, string mime_type)
{
	Attachment a;
	a.file = dcp::ArrayData(file);
	a.name = name;
	a.mime_type = mime_type;
	_attachments.push_back(a);
}


static size_t
curl_data_shim(void* ptr, size_t size, size_t nmemb, void* userp)
{
	return reinterpret_cast<Email*>(userp)->get_data(ptr, size, nmemb);
}


static int
curl_debug_shim(CURL* curl, curl_infotype type, char* data, size_t size, void* userp)
{
	return reinterpret_cast<Email*>(userp)->debug(curl, type, data, size);
}


size_t
Email::get_data(void* ptr, size_t size, size_t nmemb)
{
	size_t const t = min(_email.length() - _offset, size * nmemb);
	memcpy(ptr, _email.substr(_offset, t).c_str(), t);
	_offset += t;
	return t;
}


void
Email::send_with_retry(string server, int port, EmailProtocol protocol, int retries, string user, string password)
{
	int this_try = 0;
	while (true) {
		try {
			send(server, port, protocol, user, password);
			return;
		} catch (NetworkError& e) {
			LOG_ERROR("Error {} when trying to send email on attempt {} of 3", e.what(), this_try + 1, retries);
			if (this_try == (retries - 1)) {
				throw;
			}
		}
		++this_try;
	}
}


void
Email::send(string server, int port, EmailProtocol protocol, string user, string password)
{
	_email = "Date: " + rfc_2822_date(time(nullptr)) + "\r\n"
		"To: " + address_list(_to) + "\r\n"
		"From: " + _from + "\r\n";

	if (!_cc.empty()) {
		_email += "Cc: " + address_list(_cc) + "\r\n";
	}

	if (!_bcc.empty()) {
		_email += "Bcc: " + address_list(_bcc) + "\r\n";
	}

	string const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	string boundary;
	for (int i = 0; i < 32; ++i) {
		boundary += chars[rand() % chars.length()];
	}

	if (!_attachments.empty()) {
		_email += "MIME-Version: 1.0\r\n"
			"Content-Type: multipart/mixed; boundary=" + boundary + "\r\n";
	}

	_email += "Subject: " + encode_rfc1342(_subject) + "\r\n" +
		variant::insert_dcpomatic("User-Agent: {}\r\n\r\n");

	if (!_attachments.empty()) {
		_email += "--" + boundary + "\r\n"
			+ "Content-Type: text/plain; charset=utf-8\r\n\r\n";
	}

	_email += _body;

	for (auto i: _attachments) {
		_email += "\r\n\r\n--" + boundary + "\r\n"
			"Content-Type: " + i.mime_type + "; name=" + encode_rfc1342(i.name) + "\r\n"
			"Content-Transfer-Encoding: Base64\r\n"
			"Content-Disposition: attachment; filename=" + encode_rfc1342(i.name) + "\r\n\r\n";

		auto b64 = BIO_new(BIO_f_base64());
		if (!b64) {
			throw std::bad_alloc();
		}

		auto bio = BIO_new(BIO_s_mem());
		if (!bio) {
			throw std::bad_alloc();
		}
		bio = BIO_push(b64, bio);

		BIO_write(bio, i.file.data(), i.file.size());
		(void) BIO_flush(bio);

		char* out;
		long int bytes = BIO_get_mem_data(bio, &out);
		_email += fix(string(out, bytes));

		BIO_free_all(b64);
	}

	if (!_attachments.empty()) {
		_email += "\r\n--" + boundary + "--\r\n";
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	auto curl = curl_easy_init();
	if (!curl) {
		throw NetworkError("Could not initialise libcurl");
	}

	if ((protocol == EmailProtocol::AUTO && port == 465) || protocol == EmailProtocol::SSL) {
		/* "SSL" or "Implicit TLS"; I think curl wants us to use smtps here */
		curl_easy_setopt(curl, CURLOPT_URL, fmt::format("smtps://{}:{}", server, port).c_str());
	} else {
		curl_easy_setopt(curl, CURLOPT_URL, fmt::format("smtp://{}:{}", server, port).c_str());
	}

	if (!user.empty()) {
		curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
	}
	if (!password.empty()) {
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, _from.c_str());

	struct curl_slist* recipients = nullptr;
	for (auto i: _to) {
		recipients = curl_slist_append(recipients, i.c_str());
	}
	for (auto i: _cc) {
		recipients = curl_slist_append(recipients, i.c_str());
	}
	for (auto i: _bcc) {
		recipients = curl_slist_append(recipients, i.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_data_shim);
	curl_easy_setopt(curl, CURLOPT_READDATA, this);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	if (protocol == EmailProtocol::AUTO || protocol == EmailProtocol::STARTTLS) {
		curl_easy_setopt(curl, CURLOPT_USE_SSL, (long) CURLUSESSL_TRY);
	}
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curl_debug_shim);
	curl_easy_setopt(curl, CURLOPT_DEBUGDATA, this);

	auto const r = curl_easy_perform(curl);
	if (r != CURLE_OK) {
		throw NetworkError(_("Failed to send email"), fmt::format("{} sending to {}:{}", curl_easy_strerror(r), server, port));
	}

	curl_slist_free_all(recipients);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


string
Email::address_list(vector<string> addresses)
{
	string o;
	for (auto i: addresses) {
		o += i + ", ";
	}

	return o.substr(0, o.length() - 2);
}


int
Email::debug(CURL *, curl_infotype type, char* data, size_t size)
{
	if (type == CURLINFO_TEXT) {
		_notes += string(data, size);
	} else if (type == CURLINFO_HEADER_IN) {
		_notes += "<- " + string(data, size);
	} else if (type == CURLINFO_HEADER_OUT) {
		_notes += "-> " + string(data, size);
	}
	return 0;
}


string
Email::encode_rfc1342(string subject)
{
	auto b64 = BIO_new(BIO_f_base64());
	if (!b64) {
		throw std::bad_alloc();
	}

	auto bio = BIO_new(BIO_s_mem());
	if (!bio) {
		throw std::bad_alloc();
	}

	bio = BIO_push(b64, bio);
	BIO_write(bio, subject.c_str(), subject.length());
	(void) BIO_flush(bio);

	char* out;
	long int bytes = BIO_get_mem_data(bio, &out);
	string base64_subject(out, bytes);
	BIO_free_all(b64);

	boost::algorithm::replace_all(base64_subject, "\n", "");
	return "=?utf-8?B?" + base64_subject + "?=";
}

