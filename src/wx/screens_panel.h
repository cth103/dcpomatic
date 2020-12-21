/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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

#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <wx/srchctrl.h>
#include <wx/treectrl.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <list>
#include <map>

namespace dcpomatic {
	class Screen;
}

class Cinema;

class ScreensPanel : public wxPanel
{
public:
	explicit ScreensPanel (wxWindow* parent);
	~ScreensPanel ();

	std::list<boost::shared_ptr<dcpomatic::Screen> > screens () const;
	void setup_sensitivity ();

	boost::signals2::signal<void ()> ScreensChanged;

private:
	void add_cinemas ();
	boost::optional<wxTreeItemId> add_cinema (boost::shared_ptr<Cinema>);
	boost::optional<wxTreeItemId> add_screen (boost::shared_ptr<Cinema>, boost::shared_ptr<dcpomatic::Screen>);
	void add_cinema_clicked ();
	void edit_cinema_clicked ();
	void remove_cinema_clicked ();
	void add_screen_clicked ();
	void edit_screen_clicked ();
	void remove_screen_clicked ();
	void selection_changed_shim (wxTreeEvent &);
	void selection_changed ();
	void search_changed ();

	wxSearchCtrl* _search;
	wxTreeCtrl* _targets;
	wxButton* _add_cinema;
	wxButton* _edit_cinema;
	wxButton* _remove_cinema;
	wxButton* _add_screen;
	wxButton* _edit_screen;
	wxButton* _remove_screen;
	wxTreeItemId _root;

	typedef std::map<wxTreeItemId, boost::shared_ptr<Cinema> > CinemaMap;
	typedef std::map<wxTreeItemId, boost::shared_ptr<dcpomatic::Screen> > ScreenMap;

	CinemaMap _cinemas;
	ScreenMap _screens;
	CinemaMap _selected_cinemas;
	ScreenMap _selected_screens;

	bool _ignore_selection_change;
};
