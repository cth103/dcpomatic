/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>
    Copyright (C) 2000-2007 Paul Davis

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

/** @file src/timer.h
 *  @brief Some timing classes for debugging and profiling.
 */

#ifndef DCPOMATIC_TIMER_H
#define DCPOMATIC_TIMER_H

#include <sys/time.h>
#include <string>
#include <map>

/** @class PeriodTimer
 *  @brief A class to allow timing of a period within the caller.
 *
 *  On destruction, it will output the time since its construction.
 */
class PeriodTimer
{
public:
	PeriodTimer (std::string n);
	~PeriodTimer ();
	
private:

	/** name to use when giving output */
	std::string _name;
	/** time that this class was constructed */
	struct timeval _start;
};

/** @class StateTimer
 *  @brief A class to allow measurement of the amount of time a program
 *  spends in one of a set of states.
 *
 *  Once constructed, the caller can call set_state() whenever
 *  its state changes.	When StateTimer is destroyed, it will
 *  output (to cout) a summary of the time spent in each state.
 */
class StateTimer
{
public:
	StateTimer (std::string n, std::string s);
	~StateTimer ();

	void set_state (std::string s);

private:
	/** name to add to the output */
	std::string _name;
	/** current state */
	std::string _state;
	/** time that _state was entered */
	double _time;
	/** time that has been spent in each state so far */
	std::map<std::string, double> _totals;
};

#endif
