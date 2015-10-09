/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "log.h"
#include "cross.h"
#include "config.h"
#include "safe_stringstream.h"
#include "string_log_entry.h"
#include <time.h>
#include <cstdio>

#include "i18n.h"

using std::string;
using boost::shared_ptr;

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

void
Log::log (shared_ptr<const LogEntry> e)
{
	boost::mutex::scoped_lock lm (_mutex);

	if ((_types & e->type()) == 0) {
		return;
	}

	do_log (e);
}

/** @param n String to log */
void
Log::log (string message, int type)
{
	boost::mutex::scoped_lock lm (_mutex);

	if ((_types & type) == 0) {
		return;
	}

	do_log (shared_ptr<const LogEntry> (new StringLogEntry (type, message)));
}

void
Log::dcp_log (dcp::NoteType type, string m)
{
	switch (type) {
	case dcp::DCP_PROGRESS:
		do_log (shared_ptr<const LogEntry> (new StringLogEntry (LogEntry::TYPE_GENERAL, m)));
		break;
	case dcp::DCP_ERROR:
		do_log (shared_ptr<const LogEntry> (new StringLogEntry (LogEntry::TYPE_ERROR, m)));
		break;
	case dcp::DCP_NOTE:
		do_log (shared_ptr<const LogEntry> (new StringLogEntry (LogEntry::TYPE_WARNING, m)));
		break;
	}
}

void
Log::set_types (int t)
{
	boost::mutex::scoped_lock lm (_mutex);
	_types = t;
}
