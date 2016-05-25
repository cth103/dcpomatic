/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FONT_FILES_H
#define DCPOMATIC_FONT_FILES_H

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

class FontFiles
{
public:
	enum Variant {
		NORMAL,
		ITALIC,
		BOLD,
		VARIANTS
	};

	void set (Variant variant, boost::filesystem::path file) {
		_file[variant] = file;
	}

	boost::optional<boost::filesystem::path> get (Variant variant) const {
		return _file[variant];
	}

private:
	boost::optional<boost::filesystem::path> _file[VARIANTS];
};

bool operator!= (FontFiles const & a, FontFiles const & b);

#endif
