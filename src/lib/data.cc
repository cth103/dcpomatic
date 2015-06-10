/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "data.h"
#include "cross.h"
#include "exceptions.h"

#include "i18n.h"

using boost::shared_array;

Data::Data (int size)
	: _data (new uint8_t[size])
	, _size (size)
{

}

Data::Data (uint8_t const * data, int size)
	: _data (new uint8_t[size])
	, _size (size)
{
	memcpy (_data.get(), data, size);
}

Data::Data (boost::filesystem::path file)
{
	_size = boost::filesystem::file_size (file);
	_data.reset (new uint8_t[_size]);

	FILE* f = fopen_boost (file, "rb");
	if (!f) {
		throw FileError (_("could not open file for reading"), file);
	}
	
	size_t const r = fread (_data.get(), 1, _size, f);
	if (r != size_t (_size)) {
		fclose (f);
		throw FileError (_("could not read from file"), file);
	}
		
	fclose (f);
}

void
Data::write (boost::filesystem::path file) const
{
	FILE* f = fopen_boost (file, "wb");
	if (!f) {
		throw WriteFileError (file, errno);
	}
	size_t const r = fwrite (_data.get(), 1, _size, f);
	if (r != size_t (_size)) {
		fclose (f);
		throw FileError ("could not write to file", file);
	}
	fclose (f);
}

void
Data::write_via_temp (boost::filesystem::path temp, boost::filesystem::path final) const
{
	write (temp);
	boost::filesystem::rename (temp, final);
}
