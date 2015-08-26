/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_EXCEPTION_STORE_H
#define DCPOMATIC_EXCEPTION_STORE_H

#include <boost/exception/all.hpp>
#include <boost/thread/mutex.hpp>

/** @class ExceptionStore
 *  @brief A parent class for classes which have a need to catch and
 *  re-throw exceptions.
 *
 *  This is intended for classes which run their own thread; they should do
 *  something like
 *
 *  void my_thread ()
 *  try {
 *    // do things which might throw exceptions
 *  } catch (...) {
 *    store_current ();
 *  }
 *
 *  and then in another thread call rethrow().  If any
 *  exception was thrown by my_thread it will be stored by
 *  store_current() and then rethrow() will re-throw it where
 *  it can be handled.
 */
class ExceptionStore
{
public:
	void rethrow () {
		boost::mutex::scoped_lock lm (_mutex);
		if (_exception) {
			boost::exception_ptr tmp = _exception;
			_exception = boost::exception_ptr ();
			boost::rethrow_exception (tmp);
		}
	}

protected:

	void store_current () {
		boost::mutex::scoped_lock lm (_mutex);
		_exception = boost::current_exception ();
	}

private:
	boost::exception_ptr _exception;
	mutable boost::mutex _mutex;
};

#endif
