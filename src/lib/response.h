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


#ifndef DCPOMATIC_RESPONSE_H
#define DCPOMATIC_RESPONSE_H


#include <memory>
#include <string>
#include <vector>


class Socket;


class Response
{
public:
	enum class Type {
		HTML,
		JSON,
		CSS,
		PNG
	};

	Response(int code);
	Response(int code, std::string payload, Type type = Type::HTML);

	void add_header(std::string key, std::string value);
	void set_type(Type type) {
		_type = type;
	}

	void send(std::shared_ptr<Socket> socket);

	/* Bad request */
	static Response ERROR_400;
	/* Not found */
	static Response ERROR_404;
	/* Internal server error */
	static Response ERROR_500;

private:
	int _code;

	Type _type = Type::HTML;
	std::string _payload;
	std::vector<std::pair<std::string, std::string>> _headers;
};


#endif

