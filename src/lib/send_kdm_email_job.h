/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "job.h"
#include "kdm_name_format.h"
#include <dcp/types.h>
#include <boost/filesystem.hpp>

class Screen;
class CinemaKDMs;
class Log;

class SendKDMEmailJob : public Job
{
public:
	SendKDMEmailJob (
		std::list<CinemaKDMs> cinema_kdms,
		KDMNameFormat name_format,
		NameFormat::Map name_values,
		std::string cpl_name,
		boost::shared_ptr<Log> log
		);

	std::string name () const;
	std::string json_name () const;
	void run ();

private:
	KDMNameFormat _name_format;
	NameFormat::Map _name_values;
	std::string _cpl_name;
	std::list<CinemaKDMs> _cinema_kdms;
	boost::shared_ptr<Log> _log;
};
