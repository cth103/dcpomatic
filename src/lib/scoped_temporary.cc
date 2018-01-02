/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "scoped_temporary.h"
#include "exceptions.h"
#include "cross.h"

/** Construct a ScopedTemporary.  A temporary filename is decided but the file is not opened
 *  until open() is called.
 */
ScopedTemporary::ScopedTemporary ()
	: _open (0)
{
	_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path ();
}

/** Close and delete the temporary file */
ScopedTemporary::~ScopedTemporary ()
{
	close ();
	boost::system::error_code ec;
	boost::filesystem::remove (_file, ec);
}

/** @return temporary filename */
char const *
ScopedTemporary::c_str () const
{
	return _file.string().c_str ();
}

/** Open the temporary file.
 *  @return File's FILE pointer.
 */
FILE*
ScopedTemporary::open (char const * params)
{
	close ();
	_open = fopen_boost (_file, params);
	if (!_open) {
		throw FileError ("Could not open scoped temporary", _file);
	}
	return _open;
}

/** Close the file */
void
ScopedTemporary::close ()
{
	if (_open) {
		fclose (_open);
		_open = 0;
	}
}
