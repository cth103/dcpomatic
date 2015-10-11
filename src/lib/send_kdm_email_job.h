/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "job.h"
#include <dcp/types.h>
#include <boost/filesystem.hpp>

class Screen;
class CinemaKDMs;
class Log;

class SendKDMEmailJob : public Job
{
public:
	SendKDMEmailJob (
		std::string film_name,
		std::string cpl_name,
		boost::posix_time::ptime from,
		boost::posix_time::ptime to,
		std::list<CinemaKDMs> cinema_kdms,
		boost::shared_ptr<Log> log
		);

	std::string name () const;
	std::string json_name () const;
	void run ();

private:
	std::string _film_name;
	std::string _cpl_name;
	boost::posix_time::ptime _from;
	boost::posix_time::ptime _to;
	std::list<CinemaKDMs> _cinema_kdms;
	boost::shared_ptr<Log> _log;
};
