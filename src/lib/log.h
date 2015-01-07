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

	static const int TYPE_GENERAL;
	static const int TYPE_WARNING;
	static const int TYPE_ERROR;
	static const int TYPE_DEBUG;
	static const int TYPE_TIMING;

	void log (std::string message, int type);
	void microsecond_log (std::string message, int type);
	void dcp_log (dcp::NoteType type, std::string message);

	void set_types (int types);

	virtual std::string head_and_tail (int amount = 1024) const = 0;

protected:
	
	/** mutex to protect the log */
	mutable boost::mutex _mutex;
	
private:
	virtual void do_log (std::string m) = 0;
	void config_changed ();
	
	/** bit-field of log types which should be put into the log (others are ignored) */
	int _types;
	boost::signals2::scoped_connection _config_connection;
};

class FileLog : public Log
{
public:
	FileLog (boost::filesystem::path file);

	std::string head_and_tail (int amount = 1024) const;

private:
	void do_log (std::string m);
	/** filename to write to */
	boost::filesystem::path _file;
};

class NullLog : public Log
{
public:
	std::string head_and_tail (int) const {
		return "";
	}

private:	
	void do_log (std::string) {}
};

#endif
