/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/checker.h
 *  @brief Checker class.
 */

#ifndef DCPOMATIC_CHECKER_H
#define DCPOMATIC_CHECKER_H

#include "signaller.h"
#include <boost/signals2.hpp>

/** Parent for classes which check some condition every so often and signal
 *  when the state of the condition changes.
 */
class Checker : public Signaller, public boost::noncopyable
{
public:
	virtual ~Checker ();

	void run ();

	bool ok () const;

	/** Emitted when the state of our condition changes */
	boost::signals2::signal<void (void)> StateChanged;

protected:

	Checker (int period);

	/** @return true if the condition is `ok', otherwise false */
	virtual bool check () const = 0;

private:

	void thread ();

	boost::thread* _thread;
	mutable boost::mutex _mutex;
	bool _terminate;
	bool _ok;
	/** check period in seconds */
	int _period;
};

#endif
