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

#include <boost/shared_array.hpp>
#include <boost/filesystem.hpp>
#include <stdint.h>

class Data
{
public:
	Data (int size);
	Data (uint8_t const * data, int size);
	Data (boost::filesystem::path file);

	virtual ~Data () {}

	void write (boost::filesystem::path file) const;
	void write_via_temp (boost::filesystem::path temp, boost::filesystem::path final) const;

	boost::shared_array<uint8_t> data () const {
		return _data;
	}

	int size () const {
		return _size;
	}

private:
	boost::shared_array<uint8_t> _data;
	int _size;
};
