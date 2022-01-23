/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "maths_util.h"
#include <cmath>


double
db_to_linear (double db)
{
	return pow(10, db / 20);
}


double
linear_to_db (double linear)
{
	return 20 * log10(linear);
}


float
logarithmic_fade_in_curve (float t)
{
	auto const c = clamp(t, 0.0f, 1.0f);
	return std::exp(2 * (c - 1)) * c;
}


float
logarithmic_fade_out_curve (float t)
{
	auto const c = clamp(t, 0.0f, 1.0f);
	return std::exp(-2 * c) * (1 - c);
}

