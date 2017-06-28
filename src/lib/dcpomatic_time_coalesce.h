/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "dcpomatic_time.h"
#include <iostream>

/** @param periods Set of periods in ascending order of from time */
template <class T>
std::list<TimePeriod<T> > coalesce (std::list<TimePeriod<T> > periods)
{
	bool did_something;
	std::list<TimePeriod<T> > coalesced;
	do {
		coalesced.clear ();
		did_something = false;
		for (typename std::list<TimePeriod<T> >::const_iterator i = periods.begin(); i != periods.end(); ++i) {
			typename std::list<TimePeriod<T> >::const_iterator j = i;
			++j;
			if (j != periods.end() && (i->overlap(*j) || i->to == j->from)) {
				coalesced.push_back (TimePeriod<T> (i->from, j->to));
				did_something = true;
				++i;
			} else {
				coalesced.push_back (*i);
			}
		}
		periods = coalesced;
	} while (did_something);

	return periods;
}
