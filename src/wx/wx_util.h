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

#include <list>
#include <wx/wx.h>

/** @file src/wx/wx_util.h
 *  @brief Some utility functions.
 */

extern void error_dialog (std::string);
extern wxStaticText* add_label_to_sizer (wxSizer *, wxWindow *, std::list<wxControl*>&, std::string);
extern std::string wx_to_std (wxString);
extern wxString std_to_wx (std::string);
