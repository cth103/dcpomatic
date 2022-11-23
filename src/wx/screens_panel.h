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


#include "lib/collator.h"
#include "lib/config.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/srchctrl.h>
#include <wx/treelist.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>
#include <list>
#include <map>
#include <set>


namespace dcpomatic {
	class Screen;
}


class Cinema;


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
	boost::optional<wxTreeListItem> add_cinema (std::shared_ptr<Cinema>, wxTreeListItem previous);
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
	std::shared_ptr<Cinema> cinema_for_operation () const;
	void set_screen_checked (wxTreeListItem item, bool checked);
	void setup_cinema_checked_state (wxTreeListItem screen);
	void check_all ();
	void uncheck_all ();
	bool notify_cinemas_changed();
	void clear_and_re_add();
	void config_changed(Config::Property);
	void convert_to_lower(std::string& s);
	bool matches_search(std::shared_ptr<const Cinema> cinema, std::string lower_case_search);

	std::shared_ptr<Cinema> item_to_cinema (wxTreeListItem item) const;
	std::shared_ptr<dcpomatic::Screen> item_to_screen (wxTreeListItem item) const;
	boost::optional<wxTreeListItem> cinema_to_item (std::shared_ptr<Cinema> cinema) const;
	boost::optional<wxTreeListItem> screen_to_item (std::shared_ptr<dcpomatic::Screen> screen) const;

	wxSearchCtrl* _search;
	wxTreeListCtrl* _targets;
	wxButton* _add_cinema;
	wxButton* _edit_cinema;
	wxButton* _remove_cinema;
	wxButton* _add_screen;
	wxButton* _edit_screen;
	wxButton* _remove_screen;
	wxButton* _check_all;
	wxButton* _uncheck_all;

	/* We want to be able to search (and so remove selected things from the view)
	 * but not deselect them, so we maintain lists of selected cinemas and screens.
	 */
	std::vector<std::shared_ptr<Cinema>> _selected_cinemas;
	std::vector<std::shared_ptr<dcpomatic::Screen>> _selected_screens;
	/* Likewise with checked screens, except that we can work out which cinemas
	 * are checked from which screens are checked, so we don't need to store the
	 * cinemas.
	 */
	std::set<std::shared_ptr<dcpomatic::Screen>> _checked_screens;

	std::map<wxTreeListItem, std::shared_ptr<Cinema>> _item_to_cinema;
	std::map<wxTreeListItem, std::shared_ptr<dcpomatic::Screen>> _item_to_screen;
	std::map<std::shared_ptr<Cinema>, wxTreeListItem> _cinema_to_item;
	std::map<std::shared_ptr<dcpomatic::Screen>, wxTreeListItem> _screen_to_item;

	bool _ignore_selection_change = false;
	bool _ignore_check_change = false;

	Collator _collator;

	boost::signals2::scoped_connection _config_connection;
	bool _ignore_cinemas_changed = false;
};
