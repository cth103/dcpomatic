/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

/** @file src/timer.cc
 *  @brief Some timing classes for debugging and profiling.
 */

#include "timer.h"
#include "util.h"
#include "compose.hpp"
#include <iostream>
#include <sys/time.h>

#include "i18n.h"

using namespace std;
using boost::optional;

/** @param n Name to use when giving output */
PeriodTimer::PeriodTimer (string n)
	: _name (n)
{
	gettimeofday (&_start, 0);
}

/** Destroy PeriodTimer and output the time elapsed since its construction */
PeriodTimer::~PeriodTimer ()
{
	struct timeval stop;
	gettimeofday (&stop, 0);
	cout << N_("T: ") << _name << N_(": ") << (seconds (stop) - seconds (_start)) << N_("\n");
}

StateTimer::StateTimer (string n, string s)
	: _name (n)
{
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);
	_state = s;
}

StateTimer::StateTimer (string n)
	: _name (n)
{

}

void
StateTimer::set (string s)
{
	set_internal (s);
}

void
StateTimer::set_internal (optional<string> s)
{
	double const last = _time;
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);

	if (s && _counts.find(*s) == _counts.end()) {
		_counts[*s] = Counts();
	}

	if (_state) {
		_counts[*_state].total_time += _time - last;
		_counts[*_state].number++;
	}
	_state = s;
}

void
StateTimer::unset ()
{
	set_internal (optional<string>());
}

bool compare (pair<double, string> a, pair<double, string> b)
{
	return a.first > b.first;
}

/** Destroy StateTimer and generate a summary of the state timings on cout */
StateTimer::~StateTimer ()
{
	if (!_state) {
		return;
	}

	unset ();

	int longest = 0;
	for (auto const& i: _counts) {
		longest = max (longest, int(i.first.length()));
	}

	list<pair<double, string> > sorted;

	for (auto const& i: _counts) {
		string name = i.first + string(longest + 1 - i.first.size(), ' ');
		char buffer[64];
		snprintf (buffer, 64, "%.4f", i.second.total_time);
		string total_time (buffer);
		sorted.push_back (make_pair(i.second.total_time, String::compose("\t%1%2 %3 %4", name, total_time, i.second.number, (i.second.total_time / i.second.number))));
	}

	sorted.sort (compare);

	cout << _name << N_(":\n");
	for (auto const& i: sorted) {
		cout << N_("\t") << i.second << "\n";
	}
}
