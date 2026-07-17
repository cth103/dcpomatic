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


#include "response.h"
#include "server.h"


class HTTPServer : public Server
{
public:
	explicit HTTPServer(int port, int timeout = 30);

protected:
	virtual Response get_request(std::string const& url);
	virtual Response post_request(std::string const& url, std::string const& body);
	virtual Response delete_request(std::string const& url);

private:
	void handle(std::shared_ptr<Socket> socket) override;
	Response request(std::vector<std::string> const& request, std::string const& body);

};

