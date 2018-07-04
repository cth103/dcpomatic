/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <wx/wx.h>
#include "timeline.h"

class Playlist;

class TimelineDialog : public wxDialog
{
public:
	TimelineDialog (ContentPanel *, boost::shared_ptr<Film>);

	void set_selection (ContentList selection);

private:
	void film_changed (Film::Property);
	void tool_clicked (wxCommandEvent& id);
	wxString bitmap_path (std::string name);

	boost::weak_ptr<Film> _film;
	Timeline _timeline;
	wxToolBar* _toolbar;
	wxToolBarToolBase* _snap;
	wxToolBarToolBase* _sequence;
	boost::signals2::scoped_connection _film_changed_connection;
};
