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

#include "locale_convert.h"
#include <string>
#include <inttypes.h>

using std::string;

template<>
string
locale_convert (int x, int)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%d", x);
	return buffer;
}

template<>
string
locale_convert (int64_t x, int)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%" PRId64, x);
	return buffer;
}

template<>
string
locale_convert (float x, int precision)
{
	char format[64];
	snprintf (format, sizeof(format), "%%.%df", precision);
	char buffer[64];
	snprintf (buffer, sizeof(buffer), format, x);
	return buffer;
}

template<>
string
locale_convert (double x, int precision)
{
	char format[64];
	snprintf (format, sizeof(format), "%%.%df", precision);
	char buffer[64];
	snprintf (buffer, sizeof(buffer), format, x);
	return buffer;
}

template<>
string
locale_convert (string x, int)
{
	return x;
}

template<>
string
locale_convert (char* x, int)
{
	return x;
}

template<>
string
locale_convert (char const * x, int)
{
	return x;
}

template<>
int
locale_convert (string x, int)
{
	int y = 0;
	sscanf (x.c_str(), "%d", &y);
	return y;
}

template<>
int64_t
locale_convert (string x, int)
{
	int64_t y = 0;
	sscanf (x.c_str(), "%" PRId64, &y);
	return y;
}

template<>
float
locale_convert (string x, int)
{
	float y = 0;
	sscanf (x.c_str(), "%f", &y);
	return y;
}

template<>
double
locale_convert (string x, int)
{
	double y = 0;
	sscanf (x.c_str(), "%lf", &y);
	return y;
}
