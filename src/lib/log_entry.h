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

#ifndef DCPOMATIC_LOG_ENTRY_H
#define DCPOMATIC_LOG_ENTRY_H

#include <string>

class LogEntry
{
public:

	static const int TYPE_GENERAL;
	static const int TYPE_WARNING;
	static const int TYPE_ERROR;
	static const int TYPE_DEBUG_DECODE;
	static const int TYPE_DEBUG_ENCODE;
	static const int TYPE_TIMING;

	LogEntry (int type);

	virtual std::string message () const = 0;

	int type () const {
		return _type;
	}
	std::string get () const;
	double seconds () const;

private:
	struct timeval _time;
	int _type;
};

#endif
