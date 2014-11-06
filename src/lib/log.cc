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

#include <time.h>
#include <cstdio>
#include "log.h"
#include "cross.h"
#include "config.h"
#include "safe_stringstream.h"

#include "i18n.h"

using namespace std;

int const Log::TYPE_GENERAL = 0x1;
int const Log::TYPE_WARNING = 0x2;
int const Log::TYPE_ERROR   = 0x4;
int const Log::TYPE_TIMING  = 0x8;

Log::Log ()
	: _types (0)
{
	_config_connection = Config::instance()->Changed.connect (boost::bind (&Log::config_changed, this));
	config_changed ();
}

void
Log::config_changed ()
{
	set_types (Config::instance()->log_types ());
}

/** @param n String to log */
void
Log::log (string message, int type)
{
	boost::mutex::scoped_lock lm (_mutex);

	if ((_types & type) == 0) {
		return;
	}

	time_t t;
	time (&t);
	string a = ctime (&t);

	SafeStringStream s;
	s << a.substr (0, a.length() - 1) << N_(": ");

	if (type & TYPE_ERROR) {
		s << "ERROR: ";
	}

	if (type & TYPE_WARNING) {
		s << "WARNING: ";
	}
	
	s << message;
	do_log (s.str ());
}

void
Log::microsecond_log (string m, int t)
{
	boost::mutex::scoped_lock lm (_mutex);

	if ((_types & t) == 0) {
		return;
	}

	struct timeval tv;
	gettimeofday (&tv, 0);

	SafeStringStream s;
	s << tv.tv_sec << N_(":") << tv.tv_usec << N_(" ") << m;
	do_log (s.str ());
}	

void
Log::set_types (int t)
{
	boost::mutex::scoped_lock lm (_mutex);
	_types = t;
}

/** @param file Filename to write log to */
FileLog::FileLog (boost::filesystem::path file)
	: _file (file)
{

}

void
FileLog::do_log (string m)
{
	FILE* f = fopen_boost (_file, "a");
	if (!f) {
		cout << "(could not log to " << _file.string() << "): " << m << "\n";
		return;
	}

	fprintf (f, "%s\n", m.c_str ());
	fclose (f);
}

string
FileLog::head_and_tail (int amount) const
{
	boost::mutex::scoped_lock lm (_mutex);

	uintmax_t head_amount = amount;
	uintmax_t tail_amount = amount;
	uintmax_t size = boost::filesystem::file_size (_file);

	if (size < (head_amount + tail_amount)) {
		head_amount = size;
		tail_amount = 0;
	}
	
	FILE* f = fopen_boost (_file, "r");
	if (!f) {
		return "";
	}

	string out;

	char* buffer = new char[max(head_amount, tail_amount) + 1];
	
	int N = fread (buffer, 1, head_amount, f);
	buffer[N] = '\0';
	out += string (buffer);

	if (tail_amount > 0) {
		out +=  "\n .\n .\n .\n";

		fseek (f, - tail_amount - 1, SEEK_END);
		
		N = fread (buffer, 1, tail_amount, f);
		buffer[N] = '\0';
		out += string (buffer) + "\n";
	}

	delete[] buffer;
	fclose (f);

	return out;
}
