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

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <sigc++/sigc++.h>
#include "lib/trim_action.h"

class Film;

class DCPRangeDialog : public wxDialog
{
public:
	DCPRangeDialog (wxWindow *, Film *);

	sigc::signal2<void, int, TrimAction> Changed;

private:
	void whole_toggled (wxCommandEvent &);
	void first_toggled (wxCommandEvent &);
	void cut_toggled (wxCommandEvent &);
	void n_frames_changed (wxCommandEvent &);
	
	void set_sensitivity ();
	void emit_changed ();
	
	Film* _film;
	wxRadioButton* _whole;
	wxRadioButton* _first;
	wxSpinCtrl* _n_frames;
	wxRadioButton* _cut;
	wxRadioButton* _black_out;
};
