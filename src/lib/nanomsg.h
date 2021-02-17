/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include <string>
#include <list>
#include <boost/optional.hpp>


class Nanomsg
{
public:
	explicit Nanomsg (bool server);

	NanoMsg (Nanomsg const&) = delete;
	NanoMsg& operator= (Nanomsg const&) = delete;

	/** Try to send a message, waiting for some timeout before giving up.
	 *  @param timeout Timeout in milliseconds, or -1 for infinite timeout.
	 *  @return true if the send happened, false if there was a timeout.
	 */
	bool send (std::string s, int timeout);

	/** Try to receive a message, waiting for some timeout before giving up.
	 *  @param timeout Timeout in milliseconds, or -1 for infinite timeout.
	 *  @return Empty if the timeout was reached, otherwise the received string.
	 */
	boost::optional<std::string> receive (int timeout);

private:
	boost::optional<std::string> get_from_pending ();
	void recv_and_parse (int flags);

	int _socket;
	std::list<std::string> _pending;
	std::string _current;
};

