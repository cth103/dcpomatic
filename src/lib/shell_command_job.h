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

/** @file src/shell_command_job.h
 *  @brief A job which calls a command via a shell.
 */

#ifndef DVDOMATIC_SHELL_COMMAND_JOB_H
#define DVDOMATIC_SHELL_COMMAND_JOB_H

#include "job.h"

class FilmState;
class Log;

/** @class ShellCommandJob
 *  @brief A job which calls a command via a shell.
 */
class ShellCommandJob : public Job
{
public:
	ShellCommandJob (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Log* l);

	std::string status () const;

protected:
	void command (std::string c);
};

#endif
