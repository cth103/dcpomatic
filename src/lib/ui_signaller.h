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

#ifndef DVDOMATIC_UI_SIGNALLER_H
#define DVDOMATIC_UI_SIGNALLER_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

class UISignaller
{
public:
	/** Create a UISignaller.  Must be called from the UI thread */
	UISignaller ()
		: _work (_service)
	{
		_ui_thread = boost::this_thread::get_id ();
	}
	
	template <typename T>
	void emit (T f) {
		if (boost::this_thread::get_id() == _ui_thread) {
			f ();
		} else {
			_service.post (f);
			wake_ui ();
		}
	}

	void ui_idle () {
		_service.poll ();
	}

	virtual void wake_ui () = 0;

private:
	boost::asio::io_service _service;
	boost::asio::io_service::work _work;
	boost::thread::id _ui_thread;
};

extern UISignaller* ui_signaller;

#endif
