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

#ifndef DVDOMATIC_LOG_H
#define DVDOMATIC_LOG_H

/** @file src/log.h
 *  @brief A very simple logging class.
 */

#include <string>
#include <boost/thread/mutex.hpp>

/** @class Log
 *  @brief A very simple logging class.
 */
class Log
{
public:
	Log ();
	virtual ~Log () {}

	enum Level {
		SILENT = 0,
		VERBOSE = 1,
		DEBUG = 2,
		TIMING = 3
	};

	void log (std::string m, Level l = STANDARD);
	void microsecond_log (std::string m, Level l = STANDARD);

	void set_level (Level l);

protected:	
	/** mutex to protect the log */
	boost::mutex _mutex;
	
private:
	virtual void do_log (std::string m) = 0;
	
	/** level above which to ignore log messages */
	Level _level;
};

class FileLog : public Log
{
public:
	FileLog (std::string file);

private:
	void do_log (std::string m);
	/** filename to write to */
	std::string _file;
};

#endif
