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

#include "name_format.h"
#include <boost/optional.hpp>
#include <boost/foreach.hpp>

using std::string;
using std::map;
using boost::optional;

static char
filter (char c)
{
	if (c == '/' || c == ':') {
		c = '-';
	} else if (c == ' ') {
		c = '_';
	}

	return c;
}

static string
filter (string c)
{
	string o;

	for (size_t i = 0; i < c.length(); ++i) {
		o += filter (c[i]);
	}

	return o;
}

void
NameFormat::add (string name, char placeholder, string title)
{
	_components.push_back (Component (name, placeholder, title));
}

optional<NameFormat::Component>
NameFormat::component_by_placeholder (char p) const
{
	BOOST_FOREACH (Component const & i, _components) {
		if (i.placeholder == p) {
			return i;
		}
	}

	return optional<Component> ();
}

string
NameFormat::get (Map values) const
{
	string result;
	for (size_t i = 0; i < _specification.length(); ++i) {
		bool done = false;
		if (_specification[i] == '%' && (i < _specification.length() - 1)) {
			optional<Component> c = component_by_placeholder (_specification[i + 1]);
			if (c) {
				result += filter (values[c->name]);
				++i;
				done = true;
			}
		}

		if (!done) {
			result += filter (_specification[i]);
		}
	}

	return result;
}

bool
operator== (NameFormat const & a, NameFormat const & b)
{
	return a.specification() == b.specification();
}
