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

class SendKDMEmailJob : public Job
{
public:
	SendKDMEmailJob (
		boost::shared_ptr<const Film>,
		std::list<boost::shared_ptr<Screen> >,
		boost::filesystem::path,
		boost::posix_time::ptime,
		boost::posix_time::ptime,
		dcp::Formulation
		);

	std::string name () const;
	std::string json_name () const;
	void run ();

private:
	std::list<boost::shared_ptr<Screen> > _screens;
	boost::filesystem::path _dcp;
	boost::posix_time::ptime _from;
	boost::posix_time::ptime _to;
	dcp::Formulation _formulation;
};
