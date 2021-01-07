/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

class Encoder;

/** @class TranscodeJob
 *  @brief A job which transcodes a Film to another format.
 */
class TranscodeJob : public Job
{
public:
	explicit TranscodeJob (std::shared_ptr<const Film> film);
	~TranscodeJob ();

	std::string name () const;
	std::string json_name () const;
	void run ();
	std::string status () const;

	void set_encoder (std::shared_ptr<Encoder> t);

private:
	int remaining_time () const;

	std::shared_ptr<Encoder> _encoder;
};
