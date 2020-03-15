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

#include "nanomsg.h"
#include "dcpomatic_log.h"
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <stdexcept>
#include <cerrno>

using std::string;
using std::runtime_error;
using boost::optional;

#define NANOMSG_URL "ipc:///tmp/dcpomatic.ipc"

Nanomsg::Nanomsg (bool server)
{
	_socket = nn_socket (AF_SP, NN_PAIR);
	if (_socket < 0) {
		throw runtime_error("Could not set up nanomsg socket");
	}
	if (server) {
		if (nn_bind(_socket, NANOMSG_URL) < 0) {
			throw runtime_error(String::compose("Could not bind nanomsg socket (%1)", errno));
		}
	} else {
		if (nn_connect(_socket, NANOMSG_URL) < 0) {
			throw runtime_error(String::compose("Could not connect nanomsg socket (%1)", errno));
		}
	}
}

void
Nanomsg::blocking_send (string s)
{
	int const r = nn_send (_socket, s.c_str(), s.length(), 0);
	if (r < 0) {
		throw runtime_error(String::compose("Could not send to nanomsg socket (%1)", errno));
	} else if (r != int(s.length())) {
		throw runtime_error("Could not send to nanomsg socket (message too big)");
	}
}

bool
Nanomsg::nonblocking_send (string s)
{
	int const r = nn_send (_socket, s.c_str(), s.length(), NN_DONTWAIT);
	if (r < 0) {
		if (errno == EAGAIN) {
			return false;
		}
		throw runtime_error(String::compose("Could not send to nanomsg socket (%1)", errno));
	} else if (r != int(s.length())) {
		throw runtime_error("Could not send to nanomsg socket (message too big)");
	}

	return true;
}

optional<string>
Nanomsg::get_from_pending ()
{
	if (_pending.empty()) {
		return optional<string>();
	}

	string const l = _pending.back();
	_pending.pop_back();
	return l;
}

void
Nanomsg::recv_and_parse (bool blocking)
{
	char* buf = 0;
	int const received = nn_recv (_socket, &buf, NN_MSG, blocking ? 0 : NN_DONTWAIT);
	if (received < 0)
	{
		if (!blocking && errno == EAGAIN) {
			return;
		}

		throw runtime_error ("Could not communicate with subprocess");
	}

	char* p = buf;
	for (int i = 0; i < received; ++i) {
		if (*p == '\n') {
			_pending.push_front (_current);
			_current = "";
		} else {
			_current += *p;
		}
		++p;
	}
	nn_freemsg (buf);
}

string
Nanomsg::blocking_get ()
{
	optional<string> l = get_from_pending ();
	if (l) {
		return *l;
	}

	recv_and_parse (true);

	l = get_from_pending ();
	if (!l) {
		throw runtime_error ("Could not communicate with subprocess");
	}

	return *l;
}

optional<string>
Nanomsg::nonblocking_get ()
{
	optional<string> l = get_from_pending ();
	if (l) {
		return *l;
	}

	recv_and_parse (false);
	return get_from_pending ();
}
