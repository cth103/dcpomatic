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


#include "dcpomatic_log.h"
#include "exceptions.h"
#include "nanomsg.h"
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <cerrno>
#include <stdexcept>


using std::runtime_error;
using std::string;
using boost::optional;


#define NANOMSG_URL "ipc:///tmp/dcpomatic.ipc"


Nanomsg::Nanomsg (bool server)
{
	_socket = nn_socket (AF_SP, NN_PAIR);
	if (_socket < 0) {
		throw runtime_error("Could not set up nanomsg socket");
	}
	if (server) {
		if ((_endpoint = nn_bind(_socket, NANOMSG_URL)) < 0) {
			throw runtime_error(String::compose("Could not bind nanomsg socket (%1)", errno));
		}
	} else {
		if ((_endpoint = nn_connect(_socket, NANOMSG_URL)) < 0) {
			throw runtime_error(String::compose("Could not connect nanomsg socket (%1)", errno));
		}
	}
}


Nanomsg::~Nanomsg ()
{
	nn_shutdown (_socket, _endpoint);
	nn_close (_socket);
}


bool
Nanomsg::send (string s, int timeout)
{
	if (timeout != 0) {
		nn_setsockopt (_socket, NN_SOL_SOCKET, NN_SNDTIMEO, &timeout, sizeof(int));
	}

	int const r = nn_send (_socket, s.c_str(), s.length(), timeout ? 0 : NN_DONTWAIT);
	if (r < 0) {
		if (errno == ETIMEDOUT || errno == EAGAIN) {
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
		return {};
	}

	auto const l = _pending.back();
	_pending.pop_back();
	return l;
}


void
Nanomsg::recv_and_parse (int flags)
{
	char* buf = 0;
	int const received = nn_recv (_socket, &buf, NN_MSG, flags);
	if (received < 0)
	{
		if (errno == ETIMEDOUT || errno == EAGAIN) {
			return;
		}

		LOG_DISK_NC("nn_recv failed");
		throw CommunicationFailedError ();
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


optional<string>
Nanomsg::receive (int timeout)
{
	if (timeout != 0) {
		nn_setsockopt (_socket, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(int));
	}

	auto l = get_from_pending ();
	if (l) {
		return *l;
	}

	recv_and_parse (timeout ? 0 : NN_DONTWAIT);

	return get_from_pending ();
}
