/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "screen_kdm.h"

class Cinema;

class CinemaKDMs
{
public:
	void make_zip_file (std::string name_first_part, boost::filesystem::path zip_file) const;

	static std::list<CinemaKDMs> collect (std::list<ScreenKDM> kdms);
	static void write_zip_files (std::string filename_first_part, std::list<CinemaKDMs> cinema_kdms, boost::filesystem::path directory);
	static void email (std::string filename_first_part, std::string cpl_name, std::list<CinemaKDMs> cinema_kdms, dcp::LocalTime from, dcp::LocalTime to);

	boost::shared_ptr<Cinema> cinema;
	std::list<ScreenKDM> screen_kdms;
};
