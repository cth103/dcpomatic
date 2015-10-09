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

#include "file_log.h"
#include "cross.h"
#include <cstdio>
#include <iostream>

using std::cout;
using std::string;
using std::max;
using boost::shared_ptr;

/** @param file Filename to write log to */
FileLog::FileLog (boost::filesystem::path file)
	: _file (file)
{

}

void
FileLog::do_log (shared_ptr<const LogEntry> entry)
{
	FILE* f = fopen_boost (_file, "a");
	if (!f) {
		cout << "(could not log to " << _file.string() << "): " << entry.get() << "\n";
		return;
	}

	fprintf (f, "%s\n", entry->get().c_str ());
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
