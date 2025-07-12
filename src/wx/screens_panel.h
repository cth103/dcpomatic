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


#include "lib/cinema_list.h"
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


class CheckBox;
class Cinema;


class ScreensPanel : public wxPanel
{
public:
	explicit ScreensPanel(wxWindow* parent);
	~ScreensPanel();

	/** Clear and re-fill the panel from the currently-configured database */
	void update();

	std::set<std::pair<CinemaID, ScreenID>> screens() const;
	void setup_sensitivity();

	dcp::UTCOffset best_utc_offset() const;

	boost::signals2::signal<void ()> ScreensChanged;

private:
	void add_cinemas();
	boost::optional<wxTreeListItem> add_cinema(CinemaID cinema_id, Cinema const& cinema, wxTreeListItem previous);
	boost::optional<wxTreeListItem> add_screen(CinemaID cinema_id, ScreenID screen_id, dcpomatic::Screen const& screen);
	void add_cinema_clicked();
	void edit_cinema_clicked();
	void edit_cinema(CinemaID cinema_id);
	void remove_cinema_clicked();
	void add_screen_clicked();
	void edit_screen_clicked();
	void edit_screen(CinemaID cinema_id, ScreenID screen_id);
	void remove_screen_clicked();
	void selection_changed_shim(wxTreeListEvent &);
	void selection_changed();
	void display_filter_changed();
	void checkbox_changed(wxTreeListEvent& ev);
	void item_activated(wxTreeListEvent& ev);
	boost::optional<CinemaID> cinema_for_operation() const;
	void set_screen_checked(wxTreeListItem item, bool checked);
	void setup_cinema_checked_state(wxTreeListItem screen);
	void check_all();
	void uncheck_all();
	void clear_and_re_add();
	void convert_to_lower(std::string& s);
	bool matches_search(Cinema const& cinema, std::string search);
	void setup_show_only_checked();

	boost::optional<CinemaID> item_to_cinema(wxTreeListItem item) const;
	boost::optional<std::pair<CinemaID, ScreenID>> item_to_screen(wxTreeListItem item) const;
	boost::optional<wxTreeListItem> cinema_to_item(CinemaID cinema) const;
	boost::optional<wxTreeListItem> screen_to_item(ScreenID screen) const;

	wxBoxSizer* _overall_sizer;
	wxSearchCtrl* _search;
	CheckBox* _show_only_checked;
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
	std::vector<CinemaID> _selected_cinemas;
	/* List of cinema_id, screen_id */
	std::vector<std::pair<CinemaID, ScreenID>> _selected_screens;
	/* Likewise with checked screens */
	std::set<std::pair<CinemaID, ScreenID>> _checked_screens;

	std::map<wxTreeListItem, CinemaID> _item_to_cinema;
	std::map<wxTreeListItem, std::pair<CinemaID, ScreenID>> _item_to_screen;
	std::map<CinemaID, wxTreeListItem> _cinema_to_item;
	std::map<ScreenID, wxTreeListItem> _screen_to_item;

	bool _ignore_selection_change = false;
	bool _ignore_check_change = false;

	std::unique_ptr<CinemaList> _cinema_list;

	Collator _collator;
};
