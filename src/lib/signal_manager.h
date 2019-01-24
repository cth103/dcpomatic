/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

class Signaller;

/** A class to allow signals to be emitted from non-UI threads and handled
 *  by a UI thread.
 */
class SignalManager : public boost::noncopyable
{
public:
	/** Create a SignalManager.  Must be called from the UI thread */
	SignalManager ()
		: _work (_service)
	{
		_ui_thread = boost::this_thread::get_id ();
	}

	virtual ~SignalManager () {}

	/* Do something next time the UI is idle */
	template <typename T>
	void when_idle (T f) {
		_service.post (f);
	}

	/** Call this in the UI when it is idle.
	 *  @return Number of idle handlers that were executed.
	 */
	size_t ui_idle () {
		/* This executes one of the functors that has been post()ed to _service */
		return _service.poll_one ();
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
			f ();
		} else {
			/* non-UI thread; post to the service and wake up the UI */
			_service.post (f);
			wake_ui ();
		}
	}

	friend class Signaller;

	/** A io_service which is used as the conduit for messages */
	boost::asio::io_service _service;
	/** Object required to keep io_service from stopping when it has nothing to do */
	boost::asio::io_service::work _work;
	/** The UI thread's ID */
	boost::thread::id _ui_thread;
};

extern SignalManager* signal_manager;

#endif
