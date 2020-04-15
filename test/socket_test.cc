/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "lib/server.h"
#include "lib/dcpomatic_socket.h"
#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/shared_ptr.hpp>
#include <cstring>
#include <iostream>

using boost::shared_ptr;
using boost::bind;

#define TEST_SERVER_PORT 9142
#define TEST_SERVER_BUFFER_LENGTH 1024


class TestServer : public Server
{
public:
	TestServer (bool digest)
		: Server (TEST_SERVER_PORT, 30)
		, _buffer (new uint8_t[TEST_SERVER_BUFFER_LENGTH])
		, _size (0)
		, _result (false)
		, _digest (digest)
	{
		_thread = boost::thread(bind(&TestServer::run, this));
	}

	~TestServer ()
	{
		stop ();
		_thread.join ();
		delete[] _buffer;
	}

	void expect (int size)
	{
		boost::mutex::scoped_lock lm (_mutex);
		_size = size;
	}

	uint8_t const * buffer() const {
		return _buffer;
	}

	void await ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_size) {
			_condition.wait (lm);
		}
	}

	bool result () const {
		return _result;
	}

private:
	void handle (boost::shared_ptr<Socket> socket)
	{
		boost::mutex::scoped_lock lm (_mutex);
		BOOST_REQUIRE (_size);
		if (_digest) {
			Socket::ReadDigestScope ds (socket);
			socket->read (_buffer, _size);
			_size = 0;
			_condition.notify_one ();
			_result = ds.check();
		} else {
			socket->read (_buffer, _size);
			_size = 0;
			_condition.notify_one ();
		}
	}

	boost::thread _thread;
	boost::mutex _mutex;
	boost::condition _condition;
	uint8_t* _buffer;
	int _size;
	bool _result;
	bool _digest;
};


void
send (shared_ptr<Socket> socket, char const* message)
{
	socket->write (reinterpret_cast<uint8_t const *>(message), strlen(message) + 1);
}

/** Basic test to see if Socket can send and receive data */
BOOST_AUTO_TEST_CASE (socket_basic_test)
{
	TestServer server(false);
	server.expect (13);

	shared_ptr<Socket> socket (new Socket);
	socket->connect (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), TEST_SERVER_PORT));
	send (socket, "Hello world!");

	server.await ();
	BOOST_CHECK_EQUAL(strcmp(reinterpret_cast<char const *>(server.buffer()), "Hello world!"), 0);
}


/** Check that the socket "auto-digest" creation works */
BOOST_AUTO_TEST_CASE (socket_digest_test1)
{
	TestServer server(false);
	server.expect (13 + 16);

	shared_ptr<Socket> socket(new Socket);
	socket->connect (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), TEST_SERVER_PORT));
	{
		Socket::WriteDigestScope ds(socket);
		send (socket, "Hello world!");
	}

	server.await ();
	BOOST_CHECK_EQUAL(strcmp(reinterpret_cast<char const *>(server.buffer()), "Hello world!"), 0);

	/* printf "%s\0" "Hello world!" | md5sum" in bash */
	char ref[] = "\x59\x86\x88\xed\x18\xc8\x71\xdd\x57\xb9\xb7\x9f\x4b\x03\x14\xcf";
	BOOST_CHECK_EQUAL (memcmp(server.buffer() + 13, ref, 16), 0);
}


/** Check that the socket "auto-digest" round-trip works */
BOOST_AUTO_TEST_CASE (socket_digest_test2)
{
	TestServer server(true);
	server.expect (13);

	shared_ptr<Socket> socket(new Socket);
	socket->connect (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), TEST_SERVER_PORT));
	{
		Socket::WriteDigestScope ds(socket);
		send (socket, "Hello world!");
	}

	server.await ();
	BOOST_CHECK_EQUAL(strcmp(reinterpret_cast<char const *>(server.buffer()), "Hello world!"), 0);

	BOOST_CHECK (server.result());
}

