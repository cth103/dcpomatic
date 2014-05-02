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

/** @file  src/lib/file_group.cc
 *  @brief FileGroup class.
 */

#include <cstdio>
#include <sndfile.h>
#include "file_group.h"
#include "exceptions.h"
#include "cross.h"

using std::vector;
using std::cout;

/** Construct a FileGroup with no files */
FileGroup::FileGroup ()
	: _current_path (0)
	, _current_file (0)
{

}

/** Construct a FileGroup with a single file */
FileGroup::FileGroup (boost::filesystem::path p)
	: _current_path (0)
	, _current_file (0)
{
	_paths.push_back (p);
	ensure_open_path (0);
	seek (0, SEEK_SET);
}

/** Construct a FileGroup with multiple files */
FileGroup::FileGroup (vector<boost::filesystem::path> const & p)
	: _paths (p)
	, _current_path (0)
	, _current_file (0)
{
	ensure_open_path (0);
	seek (0, SEEK_SET);
}

/** Destroy a FileGroup, closing any open file */
FileGroup::~FileGroup ()
{
	if (_current_file) {
		fclose (_current_file);
	}
}

void
FileGroup::set_paths (vector<boost::filesystem::path> const & p)
{
	_paths = p;
	ensure_open_path (0);
	seek (0, SEEK_SET);
}

/** Ensure that the given path index in the content is the _current_file */
void
FileGroup::ensure_open_path (size_t p) const
{
	if (_current_file && _current_path == p) {
		/* Already open */
		return;
	}
	
	if (_current_file) {
		fclose (_current_file);
	}

	_current_path = p;
	_current_file = fopen_boost (_paths[_current_path], "rb");
	if (_current_file == 0) {
		throw OpenFileError (_paths[_current_path]);
	}
}

int64_t
FileGroup::seek (int64_t pos, int whence) const
{
	/* Convert pos to `full_pos', which is an offset from the start
	   of all the files.
	*/
	int64_t full_pos = 0;
	switch (whence) {
	case SEEK_SET:
		full_pos = pos;
		break;
	case SEEK_CUR:
		for (size_t i = 0; i < _current_path; ++i) {
			full_pos += boost::filesystem::file_size (_paths[i]);
		}
#ifdef DCPOMATIC_WINDOWS
		full_pos += _ftelli64 (_current_file);
#else		
		full_pos += ftell (_current_file);
#endif		
		full_pos += pos;
		break;
	case SEEK_END:
		full_pos = length() - pos;
		break;
	}

	/* Seek to full_pos */
	size_t i = 0;
	int64_t sub_pos = full_pos;
	while (i < _paths.size ()) {
		boost::uintmax_t len = boost::filesystem::file_size (_paths[i]);
		if (sub_pos < int64_t (len)) {
			break;
		}
		sub_pos -= len;
		++i;
	}

	if (i == _paths.size ()) {
		return -1;
	}

	ensure_open_path (i);
	dcpomatic_fseek (_current_file, sub_pos, SEEK_SET);
	return full_pos;
}

/** Try to read some data from the current position into a buffer.
 *  @param buffer Buffer to write data into.
 *  @param amount Number of bytes to read.
 *  @return Number of bytes read, or -1 in the case of error.
 */
int
FileGroup::read (uint8_t* buffer, int amount) const
{
	int read = 0;
	while (1) {
		int const this_time = fread (buffer + read, 1, amount - read, _current_file);
		read += this_time;
		if (read == amount) {
			/* Done */
			break;
		}

		/* See if there is another file to use */
		if ((_current_path + 1) >= _paths.size()) {
			break;
		}
		ensure_open_path (_current_path + 1);
	}

	return read;
}

/** @return Combined length of all the files */
int64_t
FileGroup::length () const
{
	int64_t len = 0;
	for (size_t i = 0; i < _paths.size(); ++i) {
		len += boost::filesystem::file_size (_paths[i]);
	}

	return len;
}
