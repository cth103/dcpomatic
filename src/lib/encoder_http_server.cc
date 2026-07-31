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
#include "make_dcp.h"
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <memory>


using std::cout;
using std::dynamic_pointer_cast;
using std::string;


EncoderHTTPServer::EncoderHTTPServer(int port, int timeout)
	: HTTPServer(port, timeout)
{

}


Response
EncoderHTTPServer::get_request(string const& url)
{
	cout << "request: " << url << "\n";

	if (url == "/api/v1/jobs") {
		nlohmann::json json;
		auto jobs = JobManager::instance()->get();

		json["jobs"] = nlohmann::json::array();

		for (auto i = jobs.cbegin(); i != jobs.cend(); ++i) {
			nlohmann::json job;
			job["number"] = (*i)->number();
			if (auto transcode = dynamic_pointer_cast<const TranscodeJob>(*i)) {
				job["dcp"] = transcode->film()->dcp_name();
			}
			job["name"] = (*i)->json_name();
			if ((*i)->progress()) {
				job["progress"] = fmt::to_string((*i)->progress().get());
			} else {
				job["progress"] = "unknown";
			}
			job["status"] = (*i)->json_status();

			json["jobs"].push_back(job);
		}
		cout << "reply: " << json.dump() << "\n";
		return Response(200, json);
	}

	return Response::ERROR_404;
}


Response
EncoderHTTPServer::post_request(string const& url, string const& body)
{
	if (url == "/api/v1/jobs/add") {
		auto details = nlohmann::json::parse(body);
		nlohmann::json reply;
		if (!details.contains("film")) {
			reply["error"] = "no film specified";
			return Response(400, reply);
		}
		try {
			auto film = std::make_shared<Film>(details["film"].get<boost::filesystem::path>());
			film->read_metadata();
			auto job = make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
			nlohmann::json reply;
			reply["number"] = job->number();
			return Response(200, reply);
		} catch (std::exception& e) {
			reply["error"] = e.what();
			return Response(400, reply);
		}
	}

	return Response::ERROR_404;
}

