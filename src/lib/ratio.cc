/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "ratio.h"
#include "util.h"
#include <dcp/types.h>
#include <cfloat>

#include "i18n.h"


using std::string;
using std::vector;
using boost::optional;


vector<Ratio> Ratio::_ratios;


void
Ratio::setup_ratios()
{
	/* This must only be called once as we rely on the addresses of objects in _ratios staying the same */
	DCPOMATIC_ASSERT(_ratios.empty());

	_ratios.push_back(Ratio(float(1290) / 1080, "119", _("1.19"),              {},                      "119"));
	_ratios.push_back(Ratio(float(1350) / 1080, "125", _("1.25"),              {},                      "125"));
	_ratios.push_back(Ratio(float(1440) / 1080, "133", _("1.33 (4:3)"),        {},                      "133"));
	_ratios.push_back(Ratio(float(1485) / 1080, "138", _("1.38 (Academy)"),    {},                      "137"));
	_ratios.push_back(Ratio(float(1544) / 1080, "143", _("1.43 (IMAX)"),       {},                      "143"));
	_ratios.push_back(Ratio(float(1620) / 1080, "150", _("1.50"),              {},                      "150"));
	_ratios.push_back(Ratio(float(1800) / 1080, "166", _("1.66"),              {},                      "166"));
	_ratios.push_back(Ratio(float(1920) / 1080, "178", _("1.78 (16:9 or HD)"), {},                      "178"));
	_ratios.push_back(Ratio(float(1998) / 1080, "185", _("1.85 (Flat)"),       string(_("DCI Flat")),   "F"));
	_ratios.push_back(Ratio(float(1716) /  858, "200", _("2.00"),              {},                      "200"));
	_ratios.push_back(Ratio(float(2048) /  926, "221", _("2.21"),              {},                      "221"));
	_ratios.push_back(Ratio(float(2048) /  872, "235", _("2.35 (35mm Scope)"), {},                      "S"));
	_ratios.push_back(Ratio(float(2048) /  858, "239", _("2.39 (Scope)"),      string(_("DCI Scope")),  "S"));
	_ratios.push_back(Ratio(float(2048) / 1080, "190", _("1.90 (Full frame)"), string(_("Full frame")), "C"));
}


vector<Ratio const *>
Ratio::all()
{
	vector<Ratio const *> pointers;
	for (Ratio& ratio: _ratios) {
		pointers.push_back(&ratio);
	}
	return pointers;
}


Ratio const *
Ratio::from_id(string i)
{
	/* We removed the ratio with id 137; replace it with 138 */
	if (i == "137") {
		i = "138";
	}

	auto j = _ratios.begin();
	while (j != _ratios.end() && j->id() != i) {
		++j;
	}

	if (j == _ratios.end()) {
		return nullptr;
	}

	return &(*j);
}


/** @return Ratio corresponding to a given fractional ratio (+/- 0.01), or 0 */
Ratio const *
Ratio::from_ratio(float r)
{
	auto j = _ratios.begin();
	while (j != _ratios.end() && fabs(j->ratio() - r) > 0.01) {
		++j;
	}

	if (j == _ratios.end()) {
		return nullptr;
	}

	return &(*j);
}


Ratio const *
Ratio::nearest_from_ratio(float r)
{
	vector<Ratio>::const_iterator nearest = _ratios.end();
	float distance = FLT_MAX;

	for (auto i = _ratios.begin(); i != _ratios.end(); ++i) {
		float const d = fabs(i->ratio() - r);
		if (d < distance) {
			distance = d;
			nearest = i;
		}
	}

	DCPOMATIC_ASSERT(nearest != _ratios.end());

	return &(*nearest);
}

vector<Ratio const *>
Ratio::containers()
{
	if (Config::instance()->allow_any_container()) {
		return all();
	}

	return { Ratio::from_id("185"), Ratio::from_id("239") };
}


string
Ratio::container_nickname() const
{
	if (!_container_nickname) {
		/* Fall back to the image nickname; this just for when non-standard container
		   ratios are enabled.
		*/
		return _image_nickname;
	}

	return *_container_nickname;
}
