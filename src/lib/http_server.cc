/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "http_server.h"
#include "show_playlist.h"
#include "show_playlist_list.h"
#include "util.h"
#include "variant.h"
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <stdexcept>


using std::make_pair;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


Response Response::ERROR_404 = { 404, "<html><head><title>Error 404</title></head><body><h1>Error 404</h1></body></html>"};
Response Response::ERROR_500 = { 500, "<html><head><title>Error 500</title></head><body><h1>Error 500</h1></body></html>"};


HTTPServer::HTTPServer(int port, int timeout)
	: Server(port, timeout)
{

}



Response::Response(int code)
	: _code(code)
{

}


Response::Response(int code, string payload)
	: _code(code)
	, _payload(payload)
{

}


void
Response::add_header(string key, string value)
{
	_headers.push_back(make_pair(key, value));
}


void
Response::send(shared_ptr<Socket> socket)
{
	socket->write(fmt::format("HTTP/1.1 {} OK\r\n", _code));
	switch (_type) {
	case Type::HTML:
		socket->write("Content-Type: text/html; charset=utf-8\r\n");
		break;
	case Type::JSON:
		socket->write("Content-Type: text/json; charset=utf-8\r\n");
		break;
	}
	socket->write(fmt::format("Content-Length: {}\r\n", _payload.length()));
	for (auto const& header: _headers) {
		socket->write(fmt::format("{}: {}\r\n", header.first, header.second));
	}
	socket->write("\r\n");
	socket->write(_payload);
}


Response
HTTPServer::get(string const& url)
{
	if (url == "/") {
		auto page = dcp::file_to_string(resources_path() / "web" / "index.html");
		boost::algorithm::replace_all(page, "TITLE", variant::dcpomatic_player());
		return Response(200, page);
	} else if (url == "/api/v1/status") {
		nlohmann::json json;
		{
			boost::mutex::scoped_lock lm(_mutex);
			json["playing"] = _playing;
			json["position"] = seconds_to_hms(_position.seconds());
			json["dcp_name"] = _dcp_name;
		}
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else if (url == "/api/v1/playlists") {
		ShowPlaylistList list;
		nlohmann::json json;
		for (auto spl: list.show_playlists()) {
			json.push_back(spl.second.as_json());
		}
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else if (boost::algorithm::starts_with(url, "/api/v1/content/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		auto content = ShowPlaylistContentStore::instance()->get(parts[4]);
		if (!content) {
			return Response::ERROR_404;
		}
		/* XXX: converting to JSON this way feels a bit grotty */
		auto json = ShowPlaylistEntry(content, {}).as_json();
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else {
		LOG_HTTP("404 {}", url);
		return Response::ERROR_404;
	}
}


Response
HTTPServer::post(string const& url, string const& body)
{
	if (url == "/api/v1/play") {
		emit(boost::bind(boost::ref(Play)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (url == "/api/v1/stop") {
		emit(boost::bind(boost::ref(Stop)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 6) {
			return Response::ERROR_404;
		}
		ShowPlaylistList list;
		auto playlist_id = list.get_show_playlist_id(parts[4]);
		if (!playlist_id) {
			return Response::ERROR_404;
		}
		nlohmann::json details = nlohmann::json::parse(body);
		if (parts[5] == "insert") {
			auto content = ShowPlaylistContentStore::instance()->get(details["uuid"]);
			if (!content) {
				return Response::ERROR_404;
			}
			list.insert_entry(*playlist_id, ShowPlaylistEntry(content, {}), details["index"]);
		} else if (parts[5] == "move") {
			list.move_entry(*playlist_id, details["old_index"], details["new_index"]);
		} else if (parts[5] == "rename") {
			if (auto playlist = list.show_playlist(*playlist_id)) {
				playlist->set_name(details["name"]);
				list.update_show_playlist(*playlist_id, *playlist);
			}
		} else {
			return Response::ERROR_404;
		}
		return Response(200);
	} else {
		return Response::ERROR_404;
	}
}


Response
HTTPServer::request(vector<string> const& request, string const& body)
{
	vector<string> parts;
	boost::split(parts, request[0], boost::is_any_of(" "));
	if (parts.size() != 3) {
		return Response::ERROR_404;
	}

	try {
		if (parts[0] == "GET") {
			LOG_HTTP("GET {}", parts[1]);
			return get(parts[1]);
		} else if (parts[0] == "POST") {
			LOG_HTTP("POST {}", parts[1]);
			return post(parts[1], body);
		}
	} catch (std::exception& e) {
		LOG_ERROR("Error while handling HTTP request: {}", e.what());
	} catch (...) {
		LOG_ERROR("Unknown exception while handling HTTP request");
	}

	LOG_HTTP("500 {}", parts[0]);
	return Response::ERROR_500;
}


void
HTTPServer::handle(shared_ptr<Socket> socket)
{
	class Reader
	{
	public:
		void read_block(boost::system::error_code const& ec, uint8_t* data, std::size_t size)
		{
			if (ec.value() != boost::system::errc::success) {
				_close = true;
				_error_code = ec;
				return;
			}

			for (std::size_t i = 0; i < size; ++i) {
				if (_got_request) {
					_body += data[i];
				} else {
					if (_line.length() >= 1024) {
						_close = true;
						return;
					}
					_line += data[i];
					if (_line.length() >= 2 && _line.substr(_line.length() - 2) == "\r\n") {
						if (_line.length() == 2) {
							_got_request = true;
						} else if (_request.size() > 64) {
							_close = true;
							return;
						} else {
							_line = _line.substr(0, _line.length() - 2);
						}

						LOG_HTTP("Receive: {}", _line);
						_request.push_back(_line);
						_line = "";
					}
				}
			}
		}


		bool got_request() const {
			return _got_request;
		}

		bool close() const {
			return _close;
		}

		boost::system::error_code error_code() const {
			return _error_code;
		}

		vector<std::string> const& request() const {
			return _request;
		}

		std::string const& body() const {
			return _body;
		}

	private:
		std::string _line;
		vector<std::string> _request;
		std::string _body;
		bool _got_request = false;
		bool _close = false;
		boost::system::error_code _error_code;
	};

	Reader reader;

	vector<uint8_t> buffer(2048);
	socket->set_deadline_from_now(2);
	socket->socket().async_read_some(
		boost::asio::buffer(buffer.data(), buffer.size()),
		[&reader, &buffer, socket](boost::system::error_code const& ec, std::size_t bytes_transferred) {
			reader.read_block(ec, buffer.data(), bytes_transferred);
		});

	while (!reader.got_request() && !reader.close() && socket->is_open()) {
		socket->run();
	}

	if (reader.got_request() && !reader.close()) {
		try {
			auto response = request(reader.request(), reader.body());
			response.send(socket);
		} catch (runtime_error& e) {
			LOG_ERROR(e.what());
		}
	}

	/* I think we should keep the socket open if the client requested keep-alive, but some browsers
	 * send keep-alive then don't re-use the connection.  Since we can only accept one request at once,
	 * this blocks until our request read (above) times out.  We probably should accept multiple
	 * requests in parallel, but it's easier for now to close the socket.
	 */
	socket->close();
}
