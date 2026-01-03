/*
    Copyright (C) 2014-2022 Carl Hetherington <cth@carlh.net>

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


#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


class ScopedTemporary;


boost::optional<std::string> get_from_url(
	std::string url,
	bool pasv,
	bool skip_pasv_ip,
	std::function<boost::optional<std::string>(boost::filesystem::path, std::string)> load
	);

boost::optional<std::string> get_from_zip_url(
	std::string url,
	std::string file,
	bool pasv,
	bool skip_pasv_ip,
	std::function<boost::optional<std::string>(boost::filesystem::path, std::string)> load
	);

std::list<std::string> ls_url(std::string url);
