/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "smtp_server.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>


using std::string;
using boost::asio::ip::tcp;


void
run_smtp_server(int port, bool fail)
{
	boost::asio::io_context context;
	tcp::acceptor acceptor(context, tcp::endpoint(tcp::v4(), port));
	tcp::socket socket(context);
	acceptor.accept(socket);

	auto send = [&socket](string message) {
		boost::system::error_code error;
		boost::asio::write(socket, boost::asio::buffer(message + "\r\n"), boost::asio::transfer_all(), error);
	};

	auto receive = [&socket]() {
		boost::asio::streambuf buffer;
		boost::asio::read_until(socket, buffer, "\n");
		return string{
			std::istreambuf_iterator<char>(&buffer),
			std::istreambuf_iterator<char>()
		};
	};

	send("220 smtp.example.com ESMTP Postfix");
	/* EHLO */
	receive();
	send("250-smtp.example.com Hello mate [127.0.0.1]");
	send("250-SIZE 14680064");
	send("250-PIPELINING");
	send("250 HELP");
	/* MAIL FROM */
	receive();
	send("250 Ok");
	/* RCPT TO */
	if (fail) {
		return;
	}
	receive();
	send("250 Ok");
	/* DATA */
	receive();
	send("354 End data with <CR><LF>.<CR><LF>");
	/* Email body */
	receive();
	send("250 Ok");
	/* QUIT */
	receive();
}

