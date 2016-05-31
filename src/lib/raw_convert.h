/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_RAW_CONVERT_H
#define DCPOMATIC_RAW_CONVERT_H

#include "safe_stringstream.h"
#include <iomanip>

/** A sort-of version of boost::lexical_cast that does uses the "C"
 *  locale (i.e. no thousands separators and a . for the decimal separator).
 */
template <typename P, typename Q>
P
raw_convert (Q v, int precision = 16)
{
	SafeStringStream s;
	s.imbue (std::locale::classic ());
	s << std::setprecision (precision);
	s << v;
	/* If the s >> r below fails to convert anything, we want r to
	   be left as a defined value.  This construct (I believe) achieves
	   this by setting r to the default value of type P, even if P
	   is a POD type.
	*/
	P r = P ();
	s >> r;
	return r;
}

template <>
std::string
raw_convert<std::string, char const *> (char const * v, int);

template <>
std::string
raw_convert<std::string, char*> (char* v, int);

template <>
std::string
raw_convert<std::string, std::string> (std::string v, int);

#endif
