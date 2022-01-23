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


#include <algorithm>


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif


extern double db_to_linear (double db);
extern double linear_to_db (double linear);

/** @return linear gain according to a logarithmic curve, for fading in.
 *  t < 0:       linear gain of 0
 *  0 >= t >= 1: logarithmic fade in curve
 *  t > 1:       linear gain of 1
 */
extern float logarithmic_fade_in_curve (float t);


/** @return linear gain according to a logarithmic curve, for fading out.
 *  t > 1:       linear gain of 0
 *  0 >= t >= 1: logarithmic fade out curve
 *  t < 0:       linear gain of 1
 */
extern float logarithmic_fade_out_curve (float t);


template <class T>
T clamp (T val, T minimum, T maximum)
{
	return std::max(std::min(val, maximum), minimum);
}

