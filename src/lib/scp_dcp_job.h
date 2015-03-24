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

/** @file src/scp_dcp_job.h
 *  @brief A job to copy DCPs to a SCP-enabled server.
 */

#include "job.h"

class SCPDCPJob : public Job
{
public:
	SCPDCPJob (boost::shared_ptr<const Film>);

	std::string name () const;
	std::string json_name () const;
	void run ();
	std::string status () const;

private:
	void set_status (std::string);

	mutable boost::mutex _status_mutex;
	std::string _status;
};
