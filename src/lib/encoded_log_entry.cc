/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "encoded_log_entry.h"
#include "safe_stringstream.h"

using std::string;
using std::fixed;

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
	SafeStringStream m;
	m.precision (2);
	m << fixed
	  << "Encoded frame " << _frame << " from " << _ip << ": "
	  << "receive " << _receive << "s "
	  << "encode " << _encode << "s "
	  << "send " << _send << "s.";

	return m.str ();
}
