/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/log.cc
 *  @brief A very simple logging class.
 */

#include <fstream>
#include <time.h>
#include "log.h"

using namespace std;

Log::Log ()
	: _level (VERBOSE)
{

}

/** @param n String to log */
void
Log::log (string m, Level l)
{
	boost::mutex::scoped_lock lm (_mutex);

	if (l > _level) {
		return;
	}
	
	time_t t;
	time (&t);
	string a = ctime (&t);

	stringstream s;
	s << a.substr (0, a.length() - 1) << ": " << m;
	do_log (s.str ());
}

void
Log::set_level (Level l)
{
	boost::mutex::scoped_lock lm (_mutex);
	_level = l;
}


/** @param file Filename to write log to */
FileLog::FileLog (string file)
	: _file (file)
{

}

void
FileLog::do_log (string m)
{
	ofstream f (_file.c_str(), fstream::app);
	f << m << "\n";
}

