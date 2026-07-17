/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "http_server.h"
#include <dcp/raw_convert.h>
#include <boost/algorithm/string.hpp>


using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;


HTTPServer::HTTPServer(int port, int timeout)
	: Server(port, timeout)
{

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
			return get_request(parts[1]);
		} else if (parts[0] == "POST") {
			LOG_HTTP("POST {}", parts[1]);
			return post_request(parts[1], body);
		} else if (parts[0] == "DELETE") {
			LOG_HTTP("DELETE {}", parts[1]);
			return delete_request(parts[1]);
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

						if (boost::algorithm::starts_with(_line, "Content-Length:")) {
							vector<string> parts;
							boost::algorithm::split(parts, _line, boost::is_any_of(":"));
							if (parts.size() == 2) {
								boost::trim(parts[1]);
								_body_length = dcp::raw_convert<int>(parts[1]);
							}
						}

						LOG_HTTP("Receive: {}", _line);
						_request.push_back(_line);
						_line = "";
					}
				}
			}
		}


		bool finished() const {
			return _got_request && static_cast<int>(_body.length()) == _body_length;
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
		int _body_length = 0;
		boost::system::error_code _error_code;
	};

	Reader reader;

	vector<uint8_t> buffer(2048);
	socket->set_deadline_from_now(2);

	std::function<void ()> read;

	read = [&]() {
		socket->socket().async_read_some(
			boost::asio::buffer(buffer.data(), buffer.size()),
			[&reader, &buffer, socket, &read](boost::system::error_code const& ec, std::size_t bytes_transferred) {
				reader.read_block(ec, buffer.data(), bytes_transferred);
				read();
			});
	};

	read();

	while (!reader.finished() && !reader.close() && socket->is_open()) {
		socket->run();
	}

	if (reader.finished() && !reader.close()) {
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


Response
HTTPServer::get_request(string const& url)
{
	LOG_HTTP("404 GET {}", url);
	return Response::ERROR_404;
}


Response
HTTPServer::post_request(string const& url, string const&)
{
	LOG_HTTP("404 POST {}", url);
	return Response::ERROR_404;
}


Response
HTTPServer::delete_request(string const& url)
{
	LOG_HTTP("404 DELETE {}", url);
	return Response::ERROR_404;
}


