/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/log.cc
 *  @brief A very simple logging class.
 */

#include "log.h"
#include "cross.h"
#include "config.h"
#include "string_log_entry.h"
#include <time.h>
#include <cstdio>

#include "i18n.h"


using std::string;
using std::cout;
using std::shared_ptr;
using std::make_shared;


Log::Log ()
	: _types (0)
{

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

	auto e = make_shared<StringLogEntry>(type, message);

	do_log (e);
}

void
Log::dcp_log (dcp::NoteType type, string m)
{
	switch (type) {
	case dcp::NoteType::PROGRESS:
		do_log (make_shared<StringLogEntry>(LogEntry::TYPE_GENERAL, m));
		break;
	case dcp::NoteType::ERROR:
		do_log (make_shared<StringLogEntry>(LogEntry::TYPE_ERROR, m));
		break;
	case dcp::NoteType::NOTE:
		do_log (make_shared<StringLogEntry>(LogEntry::TYPE_WARNING, m));
		break;
	}
}

void
Log::set_types (int t)
{
	boost::mutex::scoped_lock lm (_mutex);
	_types = t;
}
