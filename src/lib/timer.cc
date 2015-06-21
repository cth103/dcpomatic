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

/** @file src/timer.cc
 *  @brief Some timing classes for debugging and profiling.
 */

#include <iostream>
#include <sys/time.h>
#include "timer.h"
#include "util.h"

#include "i18n.h"

using namespace std;

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

/** @param n Name to use when giving output.
 *  @param s Initial state.
 */
StateTimer::StateTimer (string n, string s)
	: _name (n)
{
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);
	_state = s;
}

/** @param s New state that the caller is in */
void
StateTimer::set_state (string s)
{
	double const last = _time;
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);

	if (_totals.find (s) == _totals.end ()) {
		_totals[s] = 0;
	}

	_totals[_state] += _time - last;
	_state = s;
}

/** Destroy StateTimer and generate a summary of the state timings on cout */
StateTimer::~StateTimer ()
{
	if (_state.empty ()) {
		return;
	}


	set_state (N_(""));

	cout << _name << N_(":\n");
	for (map<string, double>::iterator i = _totals.begin(); i != _totals.end(); ++i) {
		cout << N_("\t") << i->first << " " << i->second << N_("\n");
	}
}
