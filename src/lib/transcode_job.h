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

/** @file src/transcode_job.h
 *  @brief A job which transcodes from one format to another.
 */

#include "job.h"
#include <boost/shared_ptr.hpp>

class Transcoder;

/** @class TranscodeJob
 *  @brief A job which transcodes from one format to another.
 */
class TranscodeJob : public Job
{
public:
	TranscodeJob (boost::shared_ptr<const Film>);

	std::string name () const;
	std::string json_name () const;
	void run ();
	std::string status () const;

private:
	int remaining_time () const;

	boost::shared_ptr<Transcoder> _transcoder;
};
