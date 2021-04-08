/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include "kdm_with_metadata.h"
#include <dcp/types.h>
#include <dcp/name_format.h>
#include <boost/filesystem.hpp>


namespace dcpomatic {
	class Screen;
}


class Log;


class SendKDMEmailJob : public Job
{
public:
	SendKDMEmailJob (
		std::list<KDMWithMetadataPtr> kdms,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		std::string cpl_name
		);

	SendKDMEmailJob (
		std::list<std::list<KDMWithMetadataPtr>> kdms,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		std::string cpl_name
		);

	~SendKDMEmailJob ();

	std::string name () const;
	std::string json_name () const;
	void run ();

private:
	dcp::NameFormat _container_name_format;
	dcp::NameFormat _filename_format;
	std::string _cpl_name;
	std::list<std::list<KDMWithMetadataPtr>> _kdms;
};
