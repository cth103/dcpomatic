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

class Film;
class ThreadedStaticText;

class PropertiesDialog : public wxDialog
{
public:
	PropertiesDialog (wxWindow *, boost::shared_ptr<Film>);

private:
	std::string frames_already_encoded () const;

	boost::shared_ptr<Film> _film;
	wxStaticText* _frames;
	wxStaticText* _disk_for_frames;
	wxStaticText* _total_disk;
	ThreadedStaticText* _encoded;
};

