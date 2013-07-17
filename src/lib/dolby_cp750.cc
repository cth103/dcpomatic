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

#include "dolby_cp750.h"

#include "i18n.h"

using namespace std;

DolbyCP750::DolbyCP750 ()
	: SoundProcessor ("dolby_cp750", _("Dolby CP750"))
{

}

float
DolbyCP750::db_for_fader_change (float from, float to) const
{
	float db = 0;

	if (from < to) {
		if (from <= 4) {
			float const t = min (to, 4.0f);
			db += (t - from) * 20;
		}
		
		if (to > 4) {
			float const t = max (from, 4.0f);
			db += (to - t) * 3.33333333333333333;
		}
	} else {
		if (from >= 4) {
			float const t = max (to, 4.0f);
			db -= (from - t) * 3.33333333333333333;
		}

		if (to < 4) {
			float const t = min (from, 4.0f);
			db -= (t - to) * 20;
		}
	}

	return db;
}
