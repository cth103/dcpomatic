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

/** @file  src/make_mxf_job.h
 *  @brief A job that creates a MXF file from some data.
 */

#include "shell_command_job.h"

class FilmState;
class Options;

/** @class MakeMXFJob
 *  @brief A job that creates a MXF file from some data.
 */
class MakeMXFJob : public ShellCommandJob
{
public:
	enum Type {
		AUDIO,
		VIDEO
	};
	
	MakeMXFJob (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Options>, Log *, Type);

	std::string name () const;
	void run ();

private:
	Type _type;
};

