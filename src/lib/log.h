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

#ifndef DCPOMATIC_LOG_H
#define DCPOMATIC_LOG_H

/** @file src/log.h
 *  @brief A very simple logging class.
 */

#include "log_entry.h"
#include <dcp/types.h>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <string>

/** @class Log
 *  @brief A very simple logging class.
 */
class Log : public boost::noncopyable
{
public:
	Log ();
	virtual ~Log () {}

	void log (boost::shared_ptr<const LogEntry> entry);
	void log (std::string message, int type);
	void dcp_log (dcp::NoteType type, std::string message);

	void set_types (int types);

	/** @param amount Approximate number of bytes to return; the returned value
	 *  may be shorter or longer than this.
	 */
	virtual std::string head_and_tail (int amount = 1024) const = 0;

protected:

	/** mutex to protect the log */
	mutable boost::mutex _mutex;

private:
	virtual void do_log (boost::shared_ptr<const LogEntry> entry) = 0;
	void config_changed ();

	/** bit-field of log types which should be put into the log (others are ignored) */
	int _types;
	boost::signals2::scoped_connection _config_connection;
};

#endif
