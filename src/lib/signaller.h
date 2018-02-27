/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SIGNALLER_H
#define DCPOMATIC_SIGNALLER_H

#include "signal_manager.h"
#include <boost/thread/mutex.hpp>

class WrapperBase
{
public:
	WrapperBase ()
		: _valid (true)
		, _finished (false)
	{}

	virtual ~WrapperBase () {}

	/* Can be called from any thread */
	void invalidate ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		_valid = false;
	}

	bool finished () const {
		boost::mutex::scoped_lock lm (_mutex, boost::try_to_lock);
		if (!lm) {
			/* It's possible that emission of this
			   wrapper's signal causes another signal to
			   be emitted, which causes finished() on this
			   wrapper to be called (by Signaller::emit).
			   In this case, just say that the wrapper is
			   not yet finished.
			*/
			return false;
		}
		return _finished;
	}

protected:
	/* Protect _valid and _finished */
	mutable boost::mutex _mutex;
	bool _valid;
	bool _finished;
};

/** Helper class to manage lifetime of signals, specifically to address
 *  the problem where an object containing a signal is deleted before
 *  its signal is emitted.
 */
template <class T>
class Wrapper : public WrapperBase
{
public:
	explicit Wrapper (T signal)
		: _signal (signal)
	{

	}

	/* Called by the UI thread only */
	void signal ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_valid) {
			_signal ();
		}
		_finished = true;
	}

private:
	T _signal;
};

/** Parent for any class which needs to raise cross-thread signals (from non-UI
 *  to UI).  Subclasses should call, e.g. emit (boost::bind (boost::ref (MySignal), foo, bar));
 */
class Signaller
{
public:
	/* Can be called from any thread */
	virtual ~Signaller () {
		boost::mutex::scoped_lock lm (_signaller_mutex);
		for (std::list<WrapperBase*>::iterator i = _wrappers.begin(); i != _wrappers.end(); ++i) {
			(*i)->invalidate ();
		}
	}

	/* Can be called from any thread */
	template <class T>
	void emit (T signal)
	{
		Wrapper<T>* w = new Wrapper<T> (signal);
		if (signal_manager) {
			signal_manager->emit (boost::bind (&Wrapper<T>::signal, w));
		}

		boost::mutex::scoped_lock lm (_signaller_mutex);

		/* Clean up finished Wrappers */
		std::list<WrapperBase*>::iterator i = _wrappers.begin ();
		while (i != _wrappers.end ()) {
			std::list<WrapperBase*>::iterator tmp = i;
			++tmp;
			if ((*i)->finished ()) {
				delete *i;
				_wrappers.erase (i);
			}
			i = tmp;
		}

		/* Add the new one */
		_wrappers.push_back (w);
	}

private:
	/* Protect _wrappers */
	boost::mutex _signaller_mutex;
	std::list<WrapperBase*> _wrappers;
};

#endif
