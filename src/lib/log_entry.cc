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


#include "log_entry.h"
#include <inttypes.h>
#include <cstdio>

#include "i18n.h"


int const LogEntry::TYPE_GENERAL      = 0x001;
int const LogEntry::TYPE_WARNING      = 0x002;
int const LogEntry::TYPE_ERROR        = 0x004;
int const LogEntry::TYPE_DEBUG_THREE_D = 0x008;
int const LogEntry::TYPE_DEBUG_ENCODE = 0x010;
int const LogEntry::TYPE_TIMING       = 0x020;
int const LogEntry::TYPE_DEBUG_EMAIL  = 0x040;
int const LogEntry::TYPE_DEBUG_VIDEO_VIEW = 0x080;
int const LogEntry::TYPE_DISK         = 0x100;
int const LogEntry::TYPE_DEBUG_PLAYER = 0x200;
int const LogEntry::TYPE_DEBUG_AUDIO_ANALYSIS = 0x400;


using std::string;


LogEntry::LogEntry (int type)
	: _type (type)
{
	gettimeofday (&_time, 0);
}


string
LogEntry::get () const
{
	string s;
	if (_type & TYPE_TIMING) {
		char buffer[64];
		snprintf (buffer, sizeof(buffer), "%" PRId64 ":%" PRId64 " ", static_cast<int64_t>(_time.tv_sec), static_cast<int64_t>(_time.tv_usec));
		s += buffer;
	} else {
		char buffer[64];
		time_t const sec = _time.tv_sec;
		struct tm* t = localtime (&sec);
		strftime (buffer, 64, "%c", t);
		string a (buffer);
		s += string(buffer) + N_(": ");
	}

	if (_type & TYPE_ERROR) {
		s += "ERROR: ";
	}

	if (_type & TYPE_WARNING) {
		s += "WARNING: ";
	}

	s += message ();
	return s;
}


double
LogEntry::seconds () const
{
	return _time.tv_sec + double (_time.tv_usec / 1000000);
}
