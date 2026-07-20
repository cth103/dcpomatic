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


#include "response.h"
#include "dcpomatic_socket.h"
#include <fmt/format.h>


using std::shared_ptr;
using std::string;


Response Response::ERROR_400 = { 400, "<html><head><title>Error 400</title></head><body><h1>Error 400</h1></body></html>"};
Response Response::ERROR_404 = { 404, "<html><head><title>Error 404</title></head><body><h1>Error 404</h1></body></html>"};
Response Response::ERROR_500 = { 500, "<html><head><title>Error 500</title></head><body><h1>Error 500</h1></body></html>"};


Response::Response(int code)
	: _code(code)
{

}


Response::Response(int code, string payload, Type type)
	: _code(code)
	, _type(type)
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
	case Type::CSS:
		socket->write("Content-Type: text/css; charset=utf-8\r\n");
		break;
	case Type::PNG:
		socket->write("Content-Type: image/png;\r\n");
		break;
	}
	socket->write(fmt::format("Content-Length: {}\r\n", _payload.length()));
	for (auto const& header: _headers) {
		socket->write(fmt::format("{}: {}\r\n", header.first, header.second));
	}
	socket->write("\r\n");
	socket->write(_payload);
}


