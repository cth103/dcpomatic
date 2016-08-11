/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_LOCALE_CONVERT_H
#define DCPOMATIC_LOCALE_CONVERT_H

#include <boost/static_assert.hpp>
#include <string>

template <typename P, typename Q>
P
locale_convert (Q x, int precision = 16)
{
	/* We can't write a generic version of locale_convert; all required
	   versions must be specialised.
	*/
	BOOST_STATIC_ASSERT (sizeof (Q) == 0);
}

template <>
std::string
locale_convert (int x, int);

template <>
std::string
locale_convert (int64_t x, int);

template <>
std::string
locale_convert (float x, int precision);

template <>
std::string
locale_convert (double x, int precision);

template <>
std::string
locale_convert (std::string x, int);

template <>
std::string
locale_convert (char* x, int);

template <>
std::string
locale_convert (char const * x, int);

template <>
int
locale_convert (std::string x, int);

template <>
int64_t
locale_convert (std::string x, int);

template <>
float
locale_convert (std::string x, int);

template <>
double
locale_convert (std::string x, int);

#endif
