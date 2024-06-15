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


#include "dcpomatic_socket.h"
#include "internal_player_server.h"


using std::shared_ptr;
using std::string;
using std::vector;


void
InternalPlayerServer::handle(shared_ptr<Socket> socket)
{
	try {
		uint32_t const length = socket->read_uint32();
		if (length > 65536) {
			return;
		}
		vector<uint8_t> buffer(length);
		socket->read(buffer.data(), buffer.size());
		string s(reinterpret_cast<char*>(buffer.data()));
		emit(boost::bind(boost::ref(LoadDCP), s));
		socket->write(reinterpret_cast<uint8_t const *>("OK"), 3);
	} catch (...) {

	}
}

