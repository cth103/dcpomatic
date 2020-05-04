/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "kdm_with_metadata.h"

class Cinema;
class Job;
class Log;

class CinemaKDMs
{
public:
	boost::shared_ptr<Cinema> cinema;
	std::list<KDMWithMetadataPtr > screen_kdms;
};

void make_zip_file (CinemaKDMs kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values);

std::list<CinemaKDMs> collect (std::list<KDMWithMetadataPtr > kdms);

int write_directories (
		std::list<CinemaKDMs> cinema_kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);

int write_zip_files (
		std::list<CinemaKDMs> cinema_kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);

void email (
		std::list<CinemaKDMs> cinema_kdms,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		std::string cpl_name
		);

