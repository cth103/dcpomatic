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

#include "log_entry.h"
#include "safe_stringstream.h"

#include "i18n.h"

int const LogEntry::TYPE_GENERAL      = 0x1;
int const LogEntry::TYPE_WARNING      = 0x2;
int const LogEntry::TYPE_ERROR        = 0x4;
int const LogEntry::TYPE_DEBUG_DECODE = 0x8;
int const LogEntry::TYPE_DEBUG_ENCODE = 0x10;
int const LogEntry::TYPE_TIMING       = 0x20;

using std::string;

LogEntry::LogEntry (int type)
	: _type (type)
{
	gettimeofday (&_time, 0);
}

string
LogEntry::get () const
{
	SafeStringStream s;
	if (_type & TYPE_TIMING) {
		s << _time.tv_sec << ":" << _time.tv_usec;
	} else {
		char buffer[64];
		struct tm* t = localtime (&_time.tv_sec);
		strftime (buffer, 64, "%c", t);
		string a (buffer);
		s << a.substr (0, a.length() - 1) << N_(": ");
	}

	if (_type & TYPE_ERROR) {
		s << "ERROR: ";
	}

	if (_type & TYPE_WARNING) {
		s << "WARNING: ";
	}

	s << message ();
	return s.str ();
}

double
LogEntry::seconds () const
{
	return _time.tv_sec + double (_time.tv_usec / 1000000);
}
