/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "encoded_log_entry.h"
#include <fmt/format.h>
#ifdef DCPOMATIC_LINUX
#include <pthread.h>
#endif


using std::string;


EncodedLogEntry::EncodedLogEntry(int frame, string ip, double receive, double encode, double send)
	: LogEntry(LogEntry::TYPE_GENERAL)
	, _frame(frame)
	, _ip(ip)
	, _receive(receive)
	, _encode(encode)
	, _send(send)
{
#ifdef DCPOMATIC_LINUX
	char name[16];
	pthread_getname_np(pthread_self(), name, sizeof(name));
	_thread_name = name;
#endif
}


string
EncodedLogEntry::message() const
{
	string thread_info;
#ifdef DCPOMATIC_LINUX
	thread_info = fmt::format(" on {}", _thread_name);
#endif
	return fmt::format("Encoded frame {} from {}{}: receive {:.2f}s encode {:.2f}s send {:.2f}s.", _frame, _ip.c_str(), thread_info, _receive, _encode, _send);
}
