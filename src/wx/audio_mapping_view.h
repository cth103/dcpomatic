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
#include <wx/grid.h>
#include "lib/audio_mapping.h"

class AudioMappingView : public wxPanel
{
public:
	AudioMappingView (wxWindow *);

	void set (AudioMapping);
	void set_channels (int);

	boost::signals2::signal<void (AudioMapping)> Changed;

private:
	void left_click (wxGridEvent &);
	void right_click (wxGridEvent &);
	void set_column_labels ();
	void update_cells ();

	void off ();
	void full ();
	void minus3dB ();
	void edit ();

	wxGrid* _grid;
	wxSizer* _sizer;
	AudioMapping _map;

	wxMenu* _menu;
	int _menu_row;
	int _menu_column;
};
