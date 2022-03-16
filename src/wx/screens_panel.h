/*
    Copyright (C) 2015-2022 Carl Hetherington <cth@carlh.net>

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
#include <wx/treelist.h>
#include <boost/signals2.hpp>
#include <list>
#include <map>


namespace dcpomatic {
	class Screen;
}


class Cinema;
class UCollator;


class ScreensPanel : public wxPanel
{
public:
	explicit ScreensPanel (wxWindow* parent);
	~ScreensPanel ();

	std::vector<std::shared_ptr<dcpomatic::Screen>> screens () const;
	void setup_sensitivity ();

	boost::signals2::signal<void ()> ScreensChanged;

private:
	void add_cinemas ();
	boost::optional<wxTreeListItem> add_cinema (std::shared_ptr<Cinema>);
	boost::optional<wxTreeListItem> add_screen (std::shared_ptr<Cinema>, std::shared_ptr<dcpomatic::Screen>);
	void add_cinema_clicked ();
	void edit_cinema_clicked ();
	void remove_cinema_clicked ();
	void add_screen_clicked ();
	void edit_screen_clicked ();
	void remove_screen_clicked ();
	void selection_changed_shim (wxTreeListEvent &);
	void selection_changed ();
	void search_changed ();
	void checkbox_changed (wxTreeListEvent& ev);
	boost::optional<std::pair<wxTreeListItem, std::shared_ptr<Cinema>>> cinema_for_operation () const;

	typedef std::vector<std::pair<wxTreeListItem, std::shared_ptr<Cinema>>> Cinemas;
	typedef std::vector<std::pair<wxTreeListItem, std::shared_ptr<dcpomatic::Screen>>> Screens;

	Cinemas::iterator cinema_by_tree_list_item (wxTreeListItem item);
	Screens::const_iterator screen_by_tree_list_item (wxTreeListItem item) const;

	wxSearchCtrl* _search;
	wxTreeListCtrl* _targets;
	wxButton* _add_cinema;
	wxButton* _edit_cinema;
	wxButton* _remove_cinema;
	wxButton* _add_screen;
	wxButton* _edit_screen;
	wxButton* _remove_screen;

	Cinemas _cinemas;
	Screens _screens;
	Cinemas _selected_cinemas;
	Screens _selected_screens;

	bool _ignore_selection_change;

	class Comparator : public wxTreeListItemComparator
	{
	public:
		Comparator ();
		~Comparator ();

		int Compare (wxTreeListCtrl* tree_list, unsigned, wxTreeListItem a, wxTreeListItem b) override;

	private:
		UCollator* _collator;
	};

	Comparator _comparator;
};
