/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
class Log
{
public:
	Log ();
	virtual ~Log () {}

	Log (Log const&) = delete;
	Log& operator= (Log const&) = delete;

	void log (std::shared_ptr<const LogEntry> entry);
	void log (std::string message, int type);
	void dcp_log (dcp::NoteType type, std::string message);

	void set_types (int types);
	int types () const {
		return _types;
	}

	/** @param amount Approximate number of bytes to return; the returned value
	 *  may be shorter or longer than this.
	 */
	virtual std::string head_and_tail (int amount = 1024) const {
		(void) amount;
		return "";
	}

protected:

	/** mutex to protect the log */
	mutable boost::mutex _mutex;

private:
	virtual void do_log (std::shared_ptr<const LogEntry> entry) = 0;

	/** bit-field of log types which should be put into the log (others are ignored) */
	int _types = 0;
};


#endif
