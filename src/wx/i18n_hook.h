/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_I18N_HOOK_H
#define DCPOMATIC_I18N_HOOK_H


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <map>


class I18NHook
{
public:
	I18NHook (wxWindow* window, wxString original);

	virtual void set_text (wxString text) = 0;
	virtual wxString get_text () const = 0;

	static std::map<std::string, std::string> translations () {
		return _translations;
	}

private:
	void handle (wxMouseEvent &);

	wxWindow* _window;
	wxString _original;

	static std::map<std::string, std::string> _translations;
};


#endif
