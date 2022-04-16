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


/** @file  src/lib/file_group.h
 *  @brief FileGroup class.
 */


#ifndef DCPOMATIC_FILE_GROUP_H
#define DCPOMATIC_FILE_GROUP_H


#include <dcp/file.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <vector>


/** @class FileGroup
 *  @brief A class to make a list of files behave like they were concatenated.
 */
class FileGroup
{
public:
	FileGroup ();
	explicit FileGroup (boost::filesystem::path);
	explicit FileGroup (std::vector<boost::filesystem::path> const &);

	FileGroup (FileGroup const&) = delete;
	FileGroup& operator= (FileGroup const&) = delete;

	void set_paths (std::vector<boost::filesystem::path> const &);

	int64_t seek (int64_t, int) const;
	int read (uint8_t*, int) const;
	int64_t length () const;

private:
	void ensure_open_path (size_t) const;

	std::vector<boost::filesystem::path> _paths;
	/** Index of path that we are currently reading from */
	mutable size_t _current_path = 0;
	mutable boost::optional<dcp::File> _current_file;
	mutable size_t _current_size = 0;
	mutable int64_t _position = 0;
};


#endif
