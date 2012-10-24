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

/** @file src/copy_from_dvd_job.h
 *  @brief A job to copy a film from a DVD.
 */

#include "job.h"

/** @class CopyFromDVDJob
 *  @brief A job to copy a film from a DVD using `vobcopy'.
 */
class CopyFromDVDJob : public Job
{
public:
	CopyFromDVDJob (boost::shared_ptr<Film>, boost::shared_ptr<Job> req);

	std::string name () const;
	void run ();
};
