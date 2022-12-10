/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "film.h"
#include "job.h"
#include "job_manager.h"
#include "json_server.h"
#include "transcode_job.h"
#include <dcp/raw_convert.h>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>
#include <iostream>


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using boost::asio::ip::tcp;
using boost::thread;
using dcp::raw_convert;


#define MAX_LENGTH 512


enum State {
	AWAITING_G,
	AWAITING_E,
	AWAITING_T,
	AWAITING_SPACE,
	READING_URL,
};


JSONServer::JSONServer (int port)
{
#ifdef DCPOMATIC_LINUX
	auto t = new thread (boost::bind (&JSONServer::run, this, port));
	pthread_setname_np (t->native_handle(), "json-server");
#else
	new thread (boost::bind (&JSONServer::run, this, port));
#endif
}


void
JSONServer::run (int port)
try
{
	boost::asio::io_service io_service;
	tcp::acceptor a (io_service, tcp::endpoint (tcp::v4 (), port));
	while (true) {
		try {
			auto s = make_shared<tcp::socket>(io_service);
			a.accept (*s);
			handle (s);
		}
		catch (...) {

		}
	}
}
catch (...)
{

}


void
JSONServer::handle (shared_ptr<tcp::socket> socket)
{
	string url;
	State state = AWAITING_G;

	while (true) {
		char data[MAX_LENGTH];
		boost::system::error_code error;
		size_t len = socket->read_some (boost::asio::buffer (data), error);
		if (error) {
			cout << "error.\n";
			break;
		}

		auto p = data;
		auto e = data + len;
		while (p != e) {

			auto old_state = state;
			switch (state) {
			case AWAITING_G:
				if (*p == 'G') {
					state = AWAITING_E;
				}
				break;
			case AWAITING_E:
				if (*p == 'E') {
					state = AWAITING_T;
				}
				break;
			case AWAITING_T:
				if (*p == 'T') {
					state = AWAITING_SPACE;
				}
				break;
			case AWAITING_SPACE:
				if (*p == ' ') {
					state = READING_URL;
				}
				break;
			case READING_URL:
				if (*p == ' ') {
					request (url, socket);
					state = AWAITING_G;
					url = "";
				} else {
					url += *p;
				}
				break;
			}

			if (state == old_state && state != READING_URL) {
				state = AWAITING_G;
			}

			++p;
		}
	}
}


map<string, string>
split_get_request(string url)
{
	enum {
		AWAITING_QUESTION_MARK,
		KEY,
		VALUE
	} state = AWAITING_QUESTION_MARK;

	map<string, string> r;
	string k;
	string v;
	for (size_t i = 0; i < url.length(); ++i) {
		switch (state) {
		case AWAITING_QUESTION_MARK:
			if (url[i] == '?') {
				state = KEY;
			}
			break;
		case KEY:
			if (url[i] == '=') {
				v.clear();
				state = VALUE;
			} else {
				k += url[i];
			}
			break;
		case VALUE:
			if (url[i] == '&') {
				r.insert(make_pair(k, v));
				k.clear ();
				state = KEY;
			} else {
				v += url[i];
			}
			break;
		}
	}

	if (state == VALUE) {
		r.insert (make_pair (k, v));
	}

	return r;
}


void
JSONServer::request (string url, shared_ptr<tcp::socket> socket)
{
	cout << "request: " << url << "\n";

	auto r = split_get_request (url);
	for (auto const& i: r) {
		cout << i.first << " => " << i.second << "\n";
	}

	string action;
	if (r.find ("action") != r.end ()) {
		action = r["action"];
	}

	string json;
	if (action == "status") {

		auto jobs = JobManager::instance()->get();

		json += "{ \"jobs\": [";
		for (auto i = jobs.cbegin(); i != jobs.cend(); ++i) {
			json += "{ ";

			if ((*i)->film()) {
				json += "\"dcp\": \"" + (*i)->film()->dcp_name() + "\", ";
			}

			json += "\"name\": \"" + (*i)->json_name() + "\", ";
			if ((*i)->progress()) {
				json += "\"progress\": " + raw_convert<string>((*i)->progress().get()) + ", ";
			} else {
				json += "\"progress\": unknown, ";
			}
			json += "\"status\": \"" + (*i)->json_status() + "\"";
			json += " }";

			auto j = i;
			++j;
			if (j != jobs.end ()) {
				json += ", ";
			}
		}
		json += "] }";
	}

	string reply = "HTTP/1.1 200 OK\r\n"
		"Content-Length: " + raw_convert<string>(json.length()) + "\r\n"
		"Content-Type: application/json\r\n"
		"\r\n"
		+ json + "\r\n";
	cout << "reply: " << json << "\n";
	boost::asio::write (*socket, boost::asio::buffer(reply.c_str(), reply.length()));
}
