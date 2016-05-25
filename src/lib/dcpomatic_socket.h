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

#include <boost/asio.hpp>

/** @class Socket
 *  @brief A class to wrap a boost::asio::ip::tcp::socket with some things
 *  that are useful for DCP-o-matic.
 *
 *  This class wraps some things that I could not work out how to do easily with boost;
 *  most notably, sync read/write calls with timeouts.
 */
class Socket : public boost::noncopyable
{
public:
	Socket (int timeout = 30);

	/** @return Our underlying socket */
	boost::asio::ip::tcp::socket& socket () {
		return _socket;
	}

	void connect (boost::asio::ip::tcp::endpoint);

	void write (uint32_t n);
	void write (uint8_t const * data, int size);

	void read (uint8_t* data, int size);
	uint32_t read_uint32 ();

private:
	void check ();

	Socket (Socket const &);

	boost::asio::io_service _io_service;
	boost::asio::deadline_timer _deadline;
	boost::asio::ip::tcp::socket _socket;
	int _timeout;
};
