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

/** @file src/transcode_job.h
 *  @brief A job which transcodes from one format to another.
 */

#include <boost/shared_ptr.hpp>
#include "job.h"

class Transcoder;

/** @class TranscodeJob
 *  @brief A job which transcodes from one format to another.
 */
class TranscodeJob : public Job
{
public:
	TranscodeJob (boost::shared_ptr<const Film> f);
	
	std::string name () const;
	std::string json_name () const;
	void run ();
	std::string status () const;

private:
	int remaining_time () const;

	boost::shared_ptr<Transcoder> _transcoder;
};
