/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_UPLOADER_H
#define DCPOMATIC_UPLOADER_H


#include <boost/filesystem.hpp>


class Job;


class Uploader
{
public:
	Uploader(std::function<void (std::string)> set_status, std::function<void (float)> set_progress);
	virtual ~Uploader() {}

	void upload(boost::filesystem::path directory);

protected:

	virtual void create_directory(boost::filesystem::path directory) = 0;
	virtual void upload_file(boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size) = 0;

	std::function<void (float)> _set_progress;

private:
	void upload_directory (boost::filesystem::path base, boost::filesystem::path directory, boost::uintmax_t& transferred, boost::uintmax_t total_size);
	boost::uintmax_t count_file_sizes (boost::filesystem::path) const;
	boost::filesystem::path remove_prefix (boost::filesystem::path prefix, boost::filesystem::path target) const;

	std::function<void (std::string)> _set_status;
};

#endif
