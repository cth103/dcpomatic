/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/file_group.h
 *  @brief FileGroup class.
 */

#ifndef DCPOMATIC_FILE_GROUP_H
#define DCPOMATIC_FILE_GROUP_H

#include <boost/filesystem.hpp>
#include <vector>

/** @class FileGroup
 *  @brief A class to make a list of files behave like they were concatenated.
 */
class FileGroup
{
public:
	FileGroup ();
	FileGroup (boost::filesystem::path);
	FileGroup (std::vector<boost::filesystem::path> const &);
	~FileGroup ();

	void set_paths (std::vector<boost::filesystem::path> const &);

	int64_t seek (int64_t, int) const;
	int read (uint8_t*, int) const;
	int64_t length () const;

private:
	void ensure_open_path (size_t) const;

	std::vector<boost::filesystem::path> _paths;
	/** Index of path that we are currently reading from */
	mutable size_t _current_path;
	mutable FILE* _current_file;
};

#endif
