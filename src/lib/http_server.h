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


#include "dcpomatic_time.h"
#include "server.h"
#include "signaller.h"
LIBDCP_DISABLE_WARNINGS
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS


class Response
{
public:
	Response(int code);
	Response(int code, std::string payload);

	enum class Type {
		HTML,
		JSON
	};

	void add_header(std::string key, std::string value);
	void set_type(Type type) {
		_type = type;
	}

	void send(std::shared_ptr<Socket> socket);

	static Response ERROR_404;

private:
	int _code;

	Type _type = Type::HTML;
	std::string _payload;
	std::vector<std::pair<std::string, std::string>> _headers;
};


class HTTPServer : public Server, public Signaller
{
public:
	explicit HTTPServer(int port, int timeout = 30);

	boost::signals2::signal<void ()> Play;
	boost::signals2::signal<void ()> Stop;

	void set_playing(bool playing) {
		boost::mutex::scoped_lock lm(_mutex);
		_playing = playing;
	}

	void set_position(dcpomatic::DCPTime position) {
		boost::mutex::scoped_lock lm(_mutex);
		_position = position;
	}

	void set_dcp_name(std::string name) {
		boost::mutex::scoped_lock lm(_mutex);
		_dcp_name = name;
	}

private:
	void handle(std::shared_ptr<Socket> socket) override;
	Response request(std::vector<std::string> const& request);
	Response get(std::string const& url);
	Response post(std::string const& url);

	boost::mutex _mutex;
	bool _playing = false;
	dcpomatic::DCPTime _position;
	std::string _dcp_name;
};

