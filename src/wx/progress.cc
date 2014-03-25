/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <string>
#include "progress.h"
#include "wx_util.h"

using std::string;

Progress::Progress (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);

	_gauge = new wxGauge (this, wxID_ANY, 100);
	s->Add (_gauge, 1, wxEXPAND);
	_label = new wxStaticText (this, wxID_ANY, wxT (""));
	s->Add (_label, 1, wxEXPAND);

	SetSizerAndFit (s);
}

void
Progress::set_value (int v)
{
	_gauge->SetValue (v);
	run_gui_loop ();
}

void
Progress::set_message (wxString s)
{
	_label->SetLabel (s);
	run_gui_loop ();
}

void
Progress::run_gui_loop ()
{
	while (wxTheApp->Pending ()) {
		wxTheApp->Dispatch ();
	}
}

