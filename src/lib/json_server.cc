/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "json_server.h"
#include "job_manager.h"
#include "job.h"
#include "util.h"
#include "film.h"
#include "transcode_job.h"

using std::string;
using std::stringstream;
using std::cout;
using std::map;
using std::list;
using boost::thread;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::asio::ip::tcp;

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
	new thread (boost::bind (&JSONServer::run, this, port));
}

void
JSONServer::run (int port)
try
{
	boost::asio::io_service io_service;
	tcp::acceptor a (io_service, tcp::endpoint (tcp::v4 (), port));
	while (true) {
		try {
			shared_ptr<tcp::socket> s (new tcp::socket (io_service));
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

		char* p = data;
		char* e = data + len;
		while (p != e) {

			State old_state = state;
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

void
JSONServer::request (string url, shared_ptr<tcp::socket> socket)
{
	cout << "request: " << url << "\n";
	
	map<string, string> r = split_get_request (url);
	for (map<string, string>::iterator i = r.begin(); i != r.end(); ++i) {
		cout << i->first << " => " << i->second << "\n";
	}
	
	string action;
	if (r.find ("action") != r.end ()) {
		action = r["action"];
	}
	
	stringstream json;
	if (action == "status") {
		
		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();
		
		json << "{ \"jobs\": [";
		for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {

			json << "{ ";

			if ((*i)->film()) {
				json << "\"dcp\": \"" << (*i)->film()->dcp_name() << "\", ";
			}
			
			json << "\"name\": \""   << (*i)->json_name() << "\", ";
			if ((*i)->progress ()) {
				json << "\"progress\": " << (*i)->progress().get() << ", ";
			} else {
				json << "\"progress\": unknown, ";
			}
			json << "\"status\": \"" << (*i)->json_status() << "\"";
			json << " }";
			
			list<shared_ptr<Job> >::iterator j = i;
			++j;
			if (j != jobs.end ()) {
				json << ", ";
			}
		}
		json << "] }";
		
		if (json.str().empty ()) {
			json << "{ }";
		}
	}
	
	stringstream reply;
	reply << "HTTP/1.1 200 OK\r\n"
	      << "Content-Length: " << json.str().length() << "\r\n"
	      << "Content-Type: application/json\r\n"
			      << "\r\n"
	      << json.str () << "\r\n";
	cout << "reply: " << json.str() << "\n";
	boost::asio::write (*socket, boost::asio::buffer (reply.str().c_str(), reply.str().length()));
}
