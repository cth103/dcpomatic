/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "ratio.h"
#include "util.h"
#include <dcp/types.h>
#include <cfloat>

#include "i18n.h"

using std::string;
using std::vector;
using boost::optional;

vector<Ratio const *> Ratio::_ratios;

void
Ratio::setup_ratios ()
{
	_ratios.push_back (new Ratio (float(1290) / 1080, "119", _("1.19"),              optional<string>(),      "119"));
	_ratios.push_back (new Ratio (float(1440) / 1080, "133", _("1.33 (4:3)"),        optional<string>(),      "133"));
	_ratios.push_back (new Ratio (float(1485) / 1080, "138", _("1.38 (Academy)"),    optional<string>(),      "137"));
	_ratios.push_back (new Ratio (float(1544) / 1080, "143", _("1.43 (IMAX)"),       optional<string>(),      "143"));
	_ratios.push_back (new Ratio (float(1800) / 1080, "166", _("1.66"),              optional<string>(),      "166"));
	_ratios.push_back (new Ratio (float(1920) / 1080, "178", _("1.78 (16:9 or HD)"), optional<string>(),      "178"));
	_ratios.push_back (new Ratio (float(1998) / 1080, "185", _("1.85 (Flat)"),       string(_("DCI Flat")),   "F"));
	_ratios.push_back (new Ratio (float(2048) /  872, "235", _("2.35 (35mm Scope)"), optional<string>(),      "S"));
	_ratios.push_back (new Ratio (float(2048) /  858, "239", _("2.39 (Scope)"),      string(_("DCI Scope")),  "S"));
	_ratios.push_back (new Ratio (float(2048) / 1080, "190", _("1.90 (Full frame)"), string(_("Full frame")), "C"));
}

Ratio const *
Ratio::from_id (string i)
{
	/* We removed the ratio with id 137; replace it with 138 */
	if (i == "137") {
		i = "138";
	}

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

vector<Ratio const *>
Ratio::containers ()
{
	vector<Ratio const *> r;
	r.push_back (Ratio::from_id ("185"));
	r.push_back (Ratio::from_id ("239"));
	r.push_back (Ratio::from_id ("190"));
	return r;
}

string
Ratio::container_nickname () const
{
	DCPOMATIC_ASSERT (_container_nickname);
	return *_container_nickname;
}
