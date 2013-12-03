/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include "film_editor_panel.h"

class Timecode;

class TimingPanel : public FilmEditorPanel
{
public:
	TimingPanel (FilmEditor *);

	void film_content_changed (int);
	void content_selection_changed ();
	
private:
	void position_changed ();
	void full_length_changed ();
	void trim_start_changed ();
	void trim_end_changed ();
	void play_length_changed ();
	
	Timecode* _position;
	Timecode* _full_length;
	Timecode* _trim_start;
	Timecode* _trim_end;
	Timecode* _play_length;
};
