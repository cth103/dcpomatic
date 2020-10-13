/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/file_group.cc
 *  @brief FileGroup class.
 */

#include "file_group.h"
#include "exceptions.h"
#include "cross.h"
#include "compose.hpp"
#include <sndfile.h>
#include <cstdio>
#include <iostream>

using std::vector;
using std::cout;

/** Construct a FileGroup with no files */
FileGroup::FileGroup ()
	: _current_path (0)
	, _current_file (0)
	, _current_size (0)
{

}

/** Construct a FileGroup with a single file */
FileGroup::FileGroup (boost::filesystem::path p)
	: _current_path (0)
	, _current_file (0)
	, _current_size (0)
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
		throw OpenFileError (_paths[_current_path], errno, OpenFileError::READ);
	}
	_current_size = boost::filesystem::file_size (_paths[_current_path]);
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
 *  @return Number of bytes read.
 */
int
FileGroup::read (uint8_t* buffer, int amount) const
{
	int read = 0;
	while (true) {
		int64_t to_read = amount - read;
#ifdef DCPOMATIC_WINDOWS
		/* If we over-read from the file by too much on Windows we get a errno=22 rather than an feof condition,
		 * for unknown reasons.  So if we're going to over-read, we need to do it by a little bit, so that feof
		 * still gets triggered but there is no errno=22.
		 */
		to_read = std::min(to_read, static_cast<int64_t>(_current_size - _ftelli64(_current_file) + 1));
#endif
		int const this_time = fread (buffer + read, 1, to_read, _current_file);
		read += this_time;
		if (read == amount) {
			/* Done */
			break;
		}

		if (ferror(_current_file)) {
			throw FileError (String::compose("fread error %1", errno), _paths[_current_path]);
		}

		if (feof (_current_file)) {
			/* See if there is another file to use */
			if ((_current_path + 1) >= _paths.size()) {
				break;
			}
			ensure_open_path (_current_path + 1);
		}
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
