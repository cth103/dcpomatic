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

#include <boost/signals2.hpp>

class wxTextCtrl;
class wxFocusEvent;

/** @class FocusManager class
 *  @brief A central point for notifications about when wxTextCtrls get focus in the main window.
 *
 *  This allows us to turn off accelerators for the duration of the focus so that they don't steal
 *  keypresses.  It's a hack but the only way I could make it work on all platforms (looking for
 *  the focussed thing and doing ev.Skip() if it's a wxTextCtrl did not work for me on Windows:
 *  ev.Skip() did not cause the event to be delivered).
 */
class FocusManager
{
public:
	/** emitted when any add()ed TextCtrl gets focus */
	boost::signals2::signal<void ()> SetFocus;
	/** emitted when any add()ed TextCtrl loses focus */
	boost::signals2::signal<void ()> KillFocus;

	void add(wxTextCtrl* c);

	static FocusManager* instance();

private:
	void set_focus(wxFocusEvent &);
	void kill_focus(wxFocusEvent &);

	static FocusManager* _instance;
};
