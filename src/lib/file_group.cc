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


/** @file  src/lib/file_group.cc
 *  @brief FileGroup class.
 */


#include "cross.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "file_group.h"
#include <dcp/filesystem.h>
#include <sndfile.h>
#include <cstdio>


using std::vector;


/** Construct a FileGroup with no files */
FileGroup::FileGroup ()
{

}


/** Construct a FileGroup with a single file */
FileGroup::FileGroup (boost::filesystem::path p)
{
	_paths.push_back (p);
	ensure_open_path (0);
	seek (0, SEEK_SET);
}


/** Construct a FileGroup with multiple files */
FileGroup::FileGroup (vector<boost::filesystem::path> const & p)
	: _paths (p)
{
	ensure_open_path (0);
	seek (0, SEEK_SET);
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
		_current_file->close();
	}

	auto file = dcp::File(_paths[p], "rb");
	if (!file) {
		throw OpenFileError(_paths[p], file.open_error(), OpenFileError::READ);
	}

	_current_path = p;
	_current_file = std::move(file);
	_current_size = dcp::filesystem::file_size(_paths[_current_path]);
}


int64_t
FileGroup::seek (int64_t pos, int whence) const
{
	switch (whence) {
	case SEEK_SET:
		_position = pos;
		break;
	case SEEK_CUR:
		_position += pos;
		break;
	case SEEK_END:
		_position = length() - pos;
		break;
	}

	/* Find an offset within one of the files, if _position is within a file */
	size_t i = 0;
	int64_t sub_pos = _position;
	while (i < _paths.size()) {
		boost::uintmax_t len = dcp::filesystem::file_size(_paths[i]);
		if (sub_pos < int64_t(len)) {
			break;
		}
		sub_pos -= int64_t(len);
		++i;
	}

	if (i < _paths.size()) {
		ensure_open_path (i);
		_current_file->seek(sub_pos, SEEK_SET);
	} else {
		ensure_open_path (_paths.size() - 1);
		_current_file->seek(_current_size, SEEK_SET);
	}

	return _position;
}


/** Try to read some data from the current position into a buffer.
 *  @param buffer Buffer to write data into.
 *  @param amount Number of bytes to read.
 */
FileGroup::Result
FileGroup::read (uint8_t* buffer, int amount) const
{
	DCPOMATIC_ASSERT (_current_file);

	int read = 0;
	while (true) {

		bool eof = false;
		size_t to_read = amount - read;

		DCPOMATIC_ASSERT (_current_file);

		auto const current_position = _current_file->tell();
		if (current_position == -1) {
			to_read = 0;
			eof = true;
		} else if ((current_position + to_read) > _current_size) {
			to_read = _current_size - current_position;
			eof = true;
		}

		int const this_time = _current_file->read(buffer + read, 1, to_read);
		read += this_time;
		_position += this_time;
		if (read == amount) {
			/* Done */
			break;
		}

		if (_current_file->error()) {
			throw FileError (fmt::format("fread error {}", errno), _paths[_current_path]);
		}

		if (eof) {
			/* See if there is another file to use */
			if ((_current_path + 1) >= _paths.size()) {
				return { read, true };
			}
			ensure_open_path (_current_path + 1);
		}
	}

	return { read, false };
}


/** @return Combined length of all the files */
int64_t
FileGroup::length () const
{
	int64_t len = 0;
	for (size_t i = 0; i < _paths.size(); ++i) {
		len += dcp::filesystem::file_size(_paths[i]);
	}

	return len;
}
