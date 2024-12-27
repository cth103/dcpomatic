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
#include "util.h"
#include "variant.h"
#include <boost/algorithm/string.hpp>
#include <stdexcept>


using std::make_pair;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;


Response Response::ERROR_404 = { 404, "<html><head><title>Error 404</title></head><body><h1>Error 404</h1></body></html>"};


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
		return Response(200, fmt::format(dcp::file_to_string(resources_path() / "web" / "index.html"), variant::dcpomatic_player()));
	} else if (url == "/api/v1/status") {
		auto json = string{"{ "};
		{
			boost::mutex::scoped_lock lm(_mutex);
			json += fmt::format("\"playing\": {}, ", _playing ? "true" : "false");
			json += fmt::format("\"position\": \"{}\", ", seconds_to_hms(_position.seconds()));
			json += fmt::format("\"dcp_name\": \"{}\"", _dcp_name);
		}
		json += " }";
		auto response = Response(200, json);
		response.set_type(Response::Type::JSON);
		return response;
	} else {
		LOG_HTTP("404 {}", url);
		return Response::ERROR_404;
	}
}


Response
HTTPServer::post(string const& url)
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
	} else {
		return Response::ERROR_404;
	}
}


Response
HTTPServer::request(vector<string> const& request)
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
			return post(parts[1]);
		}
	} catch (std::exception& e) {
		LOG_ERROR("Error while handling HTTP request: {}", e.what());
	} catch (...) {
		LOG_ERROR_NC("Unknown exception while handling HTTP request");
	}

	LOG_HTTP("404 {}", parts[0]);
	return Response::ERROR_404;
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
				if (_line.length() >= 1024) {
					_close = true;
					return;
				}
				_line += data[i];
				if (_line.length() >= 2 && _line.substr(_line.length() - 2) == "\r\n") {
					if (_line.length() == 2) {
						_got_request = true;
						return;
					} else if (_request.size() > 64) {
						_close = true;
						return;
					} else if (_line.size() >= 2) {
						_line = _line.substr(0, _line.length() - 2);
					}
					LOG_HTTP("Receive: {}", _line);
					_request.push_back(_line);
					_line = "";
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

		vector<std::string> const& get() const {
			return _request;
		}

	private:
		std::string _line;
		vector<std::string> _request;
		bool _got_request = false;
		bool _close = false;
		boost::system::error_code _error_code;
	};

	while (true) {

		Reader reader;

		vector<uint8_t> buffer(2048);
		socket->socket().async_read_some(
			boost::asio::buffer(buffer.data(), buffer.size()),
			[&reader, &buffer, socket](boost::system::error_code const& ec, std::size_t bytes_transferred) {
				socket->set_deadline_from_now(1);
				reader.read_block(ec, buffer.data(), bytes_transferred);
			});

		while (!reader.got_request() && !reader.close() && socket->is_open()) {
			socket->run();
		}

		if (reader.got_request() && !reader.close()) {
			try {
				auto response = request(reader.get());
				response.send(socket);
			} catch (runtime_error& e) {
				LOG_ERROR_NC(e.what());
			}
		}

		if (reader.close() || !socket->is_open()) {
			break;
		}
	}
}
