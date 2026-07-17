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
using std::vector;
using boost::asio::ip::tcp;


EncoderHTTPServer::EncoderHTTPServer(int port, int timeout)
	: HTTPServer(port, timeout)
{

}


Response
EncoderHTTPServer::get_request(string const& url)
{
	cout << "request: " << url << "\n";

	if (url == "/api/v1/status") {
		string json;
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
		cout << "reply: " << json << "\n";
		return Response(200, json, Response::Type::JSON);
	}

	return Response::ERROR_404;
}
