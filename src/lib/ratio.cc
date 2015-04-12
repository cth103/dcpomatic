/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "ratio.h"
#include "util.h"
#include <dcp/types.h>
#include <cfloat>

#include "i18n.h"

using std::string;
using std::vector;

vector<Ratio const *> Ratio::_ratios;

void
Ratio::setup_ratios ()
{
	_ratios.push_back (new Ratio (float(1290) / 1080, "119", _("1.19"), "119"));
	_ratios.push_back (new Ratio (float(1440) / 1080, "133", _("4:3"), "133"));
	_ratios.push_back (new Ratio (float(1485) / 1080, "138", _("Academy"), "137"));
	_ratios.push_back (new Ratio (float(1800) / 1080, "166", _("1.66"), "166"));
	_ratios.push_back (new Ratio (float(1920) / 1080, "178", _("16:9"), "178"));
	_ratios.push_back (new Ratio (float(1998) / 1080, "185", _("Flat"), "F"));
	_ratios.push_back (new Ratio (float(2048) /  858, "239", _("Scope"), "S"));
	_ratios.push_back (new Ratio (float(2048) / 1080, "full-frame", _("Full frame"), "C"));
}

Ratio const *
Ratio::from_id (string i)
{
	vector<Ratio const *>::iterator j = _ratios.begin ();
	while (j != _ratios.end() && (*j)->id() != i) {
		++j;
	}

	if (j == _ratios.end ()) {
		return 0;
	}

	return *j;
}

/** @return Ratio corresponding to a given fractional ratio (+/- 0.01), or 0 */
Ratio const *
Ratio::from_ratio (float r)
{
	vector<Ratio const *>::iterator j = _ratios.begin ();
	while (j != _ratios.end() && fabs ((*j)->ratio() - r) > 0.01) {
		++j;
	}

	if (j == _ratios.end ()) {
		return 0;
	}

	return *j;
}
   
Ratio const *
Ratio::nearest_from_ratio (float r)
{
	Ratio const * nearest = 0;
	float distance = FLT_MAX;
	
	for (vector<Ratio const *>::iterator i = _ratios.begin (); i != _ratios.end(); ++i) {
		float const d = fabs ((*i)->ratio() - r);
		if (d < distance) {
			distance = d;
			nearest = *i;
		}
	}

	return nearest;
}
