/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

using std::string;

EncodedLogEntry::EncodedLogEntry (int frame, string ip, double receive, double encode, double send)
	: LogEntry (LogEntry::TYPE_GENERAL)
	, _frame (frame)
	, _ip (ip)
	, _receive (receive)
	, _encode (encode)
	, _send (send)
{

}

string
EncodedLogEntry::message () const
{
	char buffer[256];
	snprintf (buffer, sizeof(buffer), "Encoded frame %d from %s: receive %.2fs encode %.2fs send %.2fs.", _frame, _ip.c_str(), _receive, _encode, _send);
	return buffer;
}
