/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "digester.h"
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>

/** @class Socket
 *  @brief A class to wrap a boost::asio::ip::tcp::socket with some things
 *  that are useful for DCP-o-matic.
 *
 *  This class wraps some things that I could not work out how to do easily with boost;
 *  most notably, sync read/write calls with timeouts.
 */
class Socket
{
public:
	explicit Socket (int timeout = 30);

	Socket (Socket const&) = delete;
	Socket& operator= (Socket const&) = delete;

	/** @return Our underlying socket */
	boost::asio::ip::tcp::socket& socket () {
		return _socket;
	}

	void connect (boost::asio::ip::tcp::endpoint);

	void write (uint32_t n);
	void write (uint8_t const * data, int size);

	void read (uint8_t* data, int size);
	uint32_t read_uint32 ();

	class ReadDigestScope
	{
	public:
		ReadDigestScope (std::shared_ptr<Socket> socket);
		bool check ();
	private:
		std::weak_ptr<Socket> _socket;
	};

	/** After one of these is created everything that is sent from the socket will be
	 *  added to a digest.  When the DigestScope is destroyed the digest will be sent
	 *  from the socket.
	 */
	class WriteDigestScope
	{
	public:
		WriteDigestScope (std::shared_ptr<Socket> socket);
		~WriteDigestScope ();
	private:
		std::weak_ptr<Socket> _socket;
	};

private:
	friend class DigestScope;

	void check ();
	void start_read_digest ();
	bool check_read_digest ();
	void start_write_digest ();
	void finish_write_digest ();

	boost::asio::io_service _io_service;
	boost::asio::deadline_timer _deadline;
	boost::asio::ip::tcp::socket _socket;
	int _timeout;
	boost::scoped_ptr<Digester> _read_digester;
	boost::scoped_ptr<Digester> _write_digester;
};
