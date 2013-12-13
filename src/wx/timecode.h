/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/signals2.hpp>
#include <wx/wx.h>
#include "lib/types.h"

class DCPTimecode : public wxPanel
{
public:
	DCPTimecode (wxWindow *);

	void set (DCPTime, int);
	DCPTime get (int) const;

	void set_editable (bool);

	boost::signals2::signal<void ()> Changed;

private:
	void changed ();
	void set_clicked ();
	
	wxSizer* _sizer;
	wxPanel* _editable;
	wxTextCtrl* _hours;
	wxStaticText* _hours_label;
	wxTextCtrl* _minutes;
	wxTextCtrl* _seconds;
	wxTextCtrl* _frames;
	wxButton* _set_button;
	wxStaticText* _fixed;
};

