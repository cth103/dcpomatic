/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_USER_PROPERTY_H
#define DCPOMATIC_USER_PROPERTY_H

#include <dcp/locale_convert.h>

class UserProperty
{
public:
	enum Category {
		GENERAL,
		VIDEO,
		AUDIO,
		LENGTH
	};

	template <class T>
	UserProperty (Category category_, std::string key_, T value_, std::string unit_ = "")
		: category (category_)
		, key (key_)
		, value (dcp::locale_convert<std::string> (value_))
		, unit (unit_)
	{}

	Category category;
	std::string key;
	std::string value;
	std::string unit;
};

#endif
