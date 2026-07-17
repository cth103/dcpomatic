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


#include "encoder_http_server.h"
#include "film.h"
#include "job_manager.h"
#include <fmt/format.h>


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using boost::asio::ip::tcp;


EncoderHTTPServer::EncoderHTTPServer(int port, int timeout)
	: HTTPServer(port, timeout)
{

}


static
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
                               k.clear();
                               state = KEY;
                       } else {
                               v += url[i];
                       }
                       break;
               }
       }

       if (state == VALUE) {
               r.insert(make_pair(k, v));
       }

       return r;
}


Response
EncoderHTTPServer::get_request(string const& url)
{
	cout << "request: " << url << "\n";

	auto r = split_get_request(url);
	for (auto const& i: r) {
		cout << i.first << " => " << i.second << "\n";
	}

	string action;
	if (r.find("action") != r.end()) {
		action = r["action"];
	}

	string json;
	if (action == "status") {

		auto jobs = JobManager::instance()->get();

		json += "{ \"jobs\": [";
		for (auto i = jobs.cbegin(); i != jobs.cend(); ++i) {
			json += "{ ";

			if (auto transcode = dynamic_pointer_cast<const TranscodeJob>(*i)) {
				json += "\"dcp\": \"" + transcode->film()->dcp_name() + "\", ";
			}

			json += "\"name\": \"" + (*i)->json_name() + "\", ";
			if ((*i)->progress()) {
				json += "\"progress\": " + fmt::to_string((*i)->progress().get()) + ", ";
			} else {
				json += "\"progress\": unknown, ";
			}
			json += "\"status\": \"" + (*i)->json_status() + "\"";
			json += " }";

			auto j = i;
			++j;
			if (j != jobs.end()) {
				json += ", ";
			}
		}
		json += "] }";
	}

	cout << "reply: " << json << "\n";
	return Response(200, json, Response::Type::JSON);
}
