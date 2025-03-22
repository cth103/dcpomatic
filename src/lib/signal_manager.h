/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_SIGNAL_MANAGER_H
#define DCPOMATIC_SIGNAL_MANAGER_H


#include "exception_store.h"
#include "io_context.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>


class Signaller;


/** A class to allow signals to be emitted from non-UI threads and handled
 *  by a UI thread.
 */
class SignalManager : public ExceptionStore
{
public:
	/** Create a SignalManager.  Must be called from the UI thread */
	SignalManager ()
		: _work(dcpomatic::make_work_guard(_context))
	{
		_ui_thread = boost::this_thread::get_id ();
	}

	virtual ~SignalManager () {}

	SignalManager (Signaller const&) = delete;
	SignalManager& operator= (Signaller const&) = delete;

	/* Do something next time the UI is idle */
	template <typename T>
	void when_idle (T f) {
		dcpomatic::post(_context, f);
	}

	/** Call this in the UI when it is idle.
	 *  @return Number of idle handlers that were executed.
	 */
	size_t ui_idle () {
		/* This executes one of the functors that has been post()ed to _context */
		return _context.poll_one();
	}

	/** This should wake the UI and make it call ui_idle() */
	virtual void wake_ui () {
		/* This is only a sensible implementation when there is no GUI */
		ui_idle ();
	}

private:
	/** Emit a signal from any thread whose handlers will be called in the UI
	 *  thread.  Use something like:
	 *
	 *  ui_signaller->emit (boost::bind (boost::ref (SomeSignal), parameter));
	 */
	template <typename T>
	void emit (T f) {
		if (boost::this_thread::get_id() == _ui_thread) {
			/* already in the UI thread */
			try {
				f ();
			} catch (...) {
				store_current ();
			}
		} else {
			/* non-UI thread; post to the context and wake up the UI */
			dcpomatic::post(_context, f);
			wake_ui ();
		}
	}

	friend class Signaller;

	/** A io_context which is used as the conduit for messages */
	dcpomatic::io_context _context;
	/** Object required to keep io_context from stopping when it has nothing to do */
	dcpomatic::work_guard _work;
	/** The UI thread's ID */
	boost::thread::id _ui_thread;
};


extern SignalManager* signal_manager;


#endif
