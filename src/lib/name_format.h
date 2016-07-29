/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_NAME_FORMAT
#define DCPOMATIC_NAME_FORMAT

#include <boost/optional.hpp>
#include <map>
#include <list>

class NameFormat
{
public:
	struct Component
	{
		Component (std::string name_, char placeholder_, std::string title_)
			: name (name_)
			, placeholder (placeholder_)
			, title (title_)
		{}

		std::string name;
		char placeholder;
		std::string title;
	};

	std::list<Component> components () const {
		return _components;
	}

	std::string specification () const {
		return _specification;
	}

	void set_specification (std::string specification) {
		_specification = specification;
	}

	typedef std::map<std::string, std::string> Map;

	std::string get (Map) const;

protected:
	NameFormat () {}

	NameFormat (std::string specification)
		: _specification (specification)
	{}

	void add (std::string name, char placeholder, std::string title);

private:
	boost::optional<NameFormat::Component> component_by_placeholder (char p) const;

	std::list<Component> _components;
	std::string _specification;
};

extern bool operator== (NameFormat const & a, NameFormat const & b);

#endif
