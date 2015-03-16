/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include "table_dialog.h"

class Film;
class ThreadedStaticText;

class PropertiesDialog : public TableDialog
{
public:
	PropertiesDialog (wxWindow *, boost::shared_ptr<Film>);

private:
	std::string frames_already_encoded () const;

	boost::shared_ptr<Film> _film;
	wxStaticText* _frames;
	wxStaticText* _disk;
	ThreadedStaticText* _encoded;

	boost::signals2::scoped_connection _encoded_connection;
};

