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

/** @file src/timer.h
 *  @brief Some timing classes for debugging and profiling.
 */

#ifndef DCPOMATIC_TIMER_H
#define DCPOMATIC_TIMER_H

#include <sys/time.h>
#include <boost/optional.hpp>
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
	explicit PeriodTimer (std::string n);
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
	explicit StateTimer (std::string n);
	StateTimer (std::string n, std::string s);
	~StateTimer ();

	void set (std::string s);
	void unset ();

	std::string name () const {
		return _name;
	}

	class Counts
	{
	public:
		Counts ()
			: total_time (0)
			, number (0)
		{}

		double total_time;
		int number;
	};

	std::map<std::string, Counts> counts () const {
		return _counts;
	}

private:
	void set_internal (boost::optional<std::string> s);

	/** name to add to the output */
	std::string _name;
	/** current state */
	boost::optional<std::string> _state;
	/** time that _state was entered */
	double _time;
	/** total time and number of entries for each state */
	std::map<std::string, Counts> _counts;
};

#endif
