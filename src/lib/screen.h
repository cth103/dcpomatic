/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <string>
#include <vector>
#include <map>
#include "util.h"

class Format;

class Screen
{
public:
	Screen (std::string);

	void set_geometry (Format const *, Position, Size);

	std::string name () const {
		return _name;
	}

	void set_name (std::string n) {
		_name = n;
	}

	struct Geometry {
		Geometry () {}

		Geometry (Position p, Size s)
			: position (p)
			, size (s)
		{}
		
		Position position;
		Size size;
	};

	typedef std::map<Format const *, Geometry> GeometryMap;
	GeometryMap geometries () const {
		return _geometries;
	}

	Position position (Format const *) const;
	Size size (Format const *) const;

	std::string as_metadata () const;
	static boost::shared_ptr<Screen> create_from_metadata (std::string);

private:
	std::string _name;
	GeometryMap _geometries;
};
