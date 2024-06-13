/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "exceptions.h"
#include <boost/bind/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <iostream>

#include "i18n.h"


using std::shared_ptr;
using std::weak_ptr;


/** @param timeout Timeout in seconds */
Socket::Socket (int timeout)
	: _deadline (_io_service)
	, _socket (_io_service)
	, _timeout (timeout)
{
	_deadline.expires_at (boost::posix_time::pos_infin);
	check ();
}

void
Socket::check ()
{
	if (_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now ()) {
		_socket.close ();
		_deadline.expires_at (boost::posix_time::pos_infin);
	}

	_deadline.async_wait (boost::bind (&Socket::check, this));
}


/** Blocking connect.
 *  @param endpoint End-point to connect to.
 */
void
Socket::connect (boost::asio::ip::tcp::endpoint endpoint)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;
	_socket.async_connect (endpoint, boost::lambda::var(ec) = boost::lambda::_1);
	do {
		_io_service.run_one();
	} while (ec == boost::asio::error::would_block);

	if (ec) {
		throw NetworkError (String::compose (_("error during async_connect (%1)"), ec.value ()));
	}

	if (!_socket.is_open ()) {
		throw NetworkError (_("connect timed out"));
	}

	if (_send_buffer_size) {
		boost::asio::socket_base::send_buffer_size old_size;
		_socket.get_option(old_size);

		boost::asio::socket_base::send_buffer_size new_size(*_send_buffer_size);
		_socket.set_option(new_size);
	}
}


/** Blocking write.
 *  @param data Buffer to write.
 *  @param size Number of bytes to write.
 */
void
Socket::write (uint8_t const * data, int size)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;

	boost::asio::async_write (_socket, boost::asio::buffer (data, size), boost::lambda::var(ec) = boost::lambda::_1);

	do {
		_io_service.run_one ();
	} while (ec == boost::asio::error::would_block);

	if (ec) {
		throw NetworkError (String::compose (_("error during async_write (%1)"), ec.value ()));
	}

	if (_write_digester) {
		_write_digester->add (data, static_cast<size_t>(size));
	}
}


void
Socket::write(std::string const& str)
{
	write(reinterpret_cast<uint8_t const*>(str.c_str()), str.size());
}


void
Socket::write (uint32_t v)
{
	v = htonl (v);
	write (reinterpret_cast<uint8_t*> (&v), 4);
}


/** Blocking read.
 *  @param data Buffer to read to.
 *  @param size Number of bytes to read.
 */
void
Socket::read (uint8_t* data, int size)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;

	boost::asio::async_read (_socket, boost::asio::buffer (data, size), boost::lambda::var(ec) = boost::lambda::_1);

	do {
		_io_service.run_one ();
	} while (ec == boost::asio::error::would_block);

	if (ec) {
		throw NetworkError (String::compose (_("error during async_read (%1)"), ec.value ()));
	}

	if (_read_digester) {
		_read_digester->add (data, static_cast<size_t>(size));
	}
}


uint32_t
Socket::read_uint32 ()
{
	uint32_t v;
	read (reinterpret_cast<uint8_t *> (&v), 4);
	return ntohl (v);
}


void
Socket::start_read_digest ()
{
	DCPOMATIC_ASSERT (!_read_digester);
	_read_digester.reset (new Digester());
}


void
Socket::start_write_digest ()
{
	DCPOMATIC_ASSERT (!_write_digester);
	_write_digester.reset (new Digester());
}


Socket::ReadDigestScope::ReadDigestScope (shared_ptr<Socket> socket)
	: _socket (socket)
{
	socket->start_read_digest ();
}


bool
Socket::ReadDigestScope::check ()
{
	auto sp = _socket.lock ();
	if (!sp) {
		return false;
	}

	return sp->check_read_digest ();
}


Socket::WriteDigestScope::WriteDigestScope (shared_ptr<Socket> socket)
	: _socket (socket)
{
	socket->start_write_digest ();
}


Socket::WriteDigestScope::~WriteDigestScope ()
{
	auto sp = _socket.lock ();
	if (sp) {
		try {
			sp->finish_write_digest ();
		} catch (...) {
			/* If we can't write our digest, something bad has happened
			 * so let's just let it happen.
			 */
		}
	}
}


bool
Socket::check_read_digest ()
{
	DCPOMATIC_ASSERT (_read_digester);
	int const size = _read_digester->size ();

	uint8_t ref[size];
	_read_digester->get (ref);

	/* Make sure _read_digester is gone before we call read() so that the digest
	 * isn't itself digested.
	 */
	_read_digester.reset ();

	uint8_t actual[size];
	read (actual, size);

	return memcmp(ref, actual, size) == 0;
}


void
Socket::finish_write_digest ()
{
	DCPOMATIC_ASSERT (_write_digester);
	int const size = _write_digester->size();

	uint8_t buffer[size];
	_write_digester->get (buffer);

	/* Make sure _write_digester is gone before we call write() so that the digest
	 * isn't itself digested.
	 */
	_write_digester.reset ();

	write (buffer, size);
}


void
Socket::set_send_buffer_size (int size)
{
	_send_buffer_size = size;
}

