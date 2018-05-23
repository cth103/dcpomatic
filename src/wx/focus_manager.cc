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

#include "focus_manager.h"
#include <wx/textctrl.h>

FocusManager* FocusManager::_instance;

FocusManager *
FocusManager::instance()
{
	if (!_instance) {
		_instance = new FocusManager();
	}

	return _instance;
}

void
FocusManager::set_focus (wxFocusEvent& ev)
{
	SetFocus();
	ev.Skip();
}

void
FocusManager::kill_focus (wxFocusEvent& ev)
{
	KillFocus();
	ev.Skip();
}

void
FocusManager::add(wxTextCtrl* c)
{
	c->Bind(wxEVT_SET_FOCUS, boost::bind(&FocusManager::set_focus, this, _1));
	c->Bind(wxEVT_KILL_FOCUS, boost::bind(&FocusManager::kill_focus, this, _1));
}
