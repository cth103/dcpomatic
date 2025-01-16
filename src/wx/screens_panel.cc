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


#include "check_box.h"
#include "cinema_dialog.h"
#include "dcpomatic_button.h"
#include "screen_dialog.h"
#include "screens_panel.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/screen.h"
#include "lib/timer.h"
#include <dcp/scope_guard.h>


using std::cout;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


ScreensPanel::ScreensPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	_overall_sizer = new wxBoxSizer(wxVERTICAL);

	auto search_sizer = new wxBoxSizer(wxHORIZONTAL);

	_search = new wxSearchCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, search_ctrl_height()));
#ifndef __WXGTK3__
	/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
	_search->ShowCancelButton (true);
#endif
	search_sizer->Add(_search, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);

	_show_only_checked = new CheckBox(this, _("Show only checked"));
	search_sizer->Add(_show_only_checked, 1, wxEXPAND | wxLEFT | wxBOTTOM, DCPOMATIC_SIZER_GAP);

	_overall_sizer->Add(search_sizer);

	auto targets = new wxBoxSizer (wxHORIZONTAL);
	_targets = new wxTreeListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_MULTIPLE | wxTL_3STATE | wxTL_NO_HEADER);
	_targets->AppendColumn(char_to_wx("foo"), 640);

	targets->Add (_targets, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	add_cinemas ();

	auto side_buttons = new wxBoxSizer (wxVERTICAL);

	auto target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_cinema = new Button (this, _("Add Cinema..."));
	target_buttons->Add (_add_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_cinema = new Button (this, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_cinema = new Button (this, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_add_screen = new Button (this, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_screen = new Button (this, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_screen = new Button (this, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

	side_buttons->Add (target_buttons, 0, 0);

	auto check_buttons = new wxBoxSizer (wxVERTICAL);

	_check_all = new Button (this, _("Check all"));
	check_buttons->Add (_check_all, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_uncheck_all = new Button (this, _("Uncheck all"));
	check_buttons->Add (_uncheck_all, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	side_buttons->Add (check_buttons, 1, wxEXPAND | wxTOP, DCPOMATIC_BUTTON_STACK_GAP * 8);

	targets->Add (side_buttons, 0, 0);

	_overall_sizer->Add (targets, 1, wxEXPAND);

	_search->Bind        (wxEVT_TEXT, boost::bind (&ScreensPanel::display_filter_changed, this));
	_show_only_checked->Bind(wxEVT_CHECKBOX, boost::bind(&ScreensPanel::display_filter_changed, this));
	_targets->Bind       (wxEVT_TREELIST_SELECTION_CHANGED, &ScreensPanel::selection_changed_shim, this);
	_targets->Bind       (wxEVT_TREELIST_ITEM_CHECKED, &ScreensPanel::checkbox_changed, this);
	_targets->Bind       (wxEVT_TREELIST_ITEM_ACTIVATED, &ScreensPanel::item_activated, this);

	_add_cinema->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_cinema_clicked, this));
	_edit_cinema->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_cinema_clicked, this));
	_remove_cinema->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_cinema_clicked, this));

	_add_screen->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_screen_clicked, this));
	_edit_screen->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_screen_clicked, this));
	_remove_screen->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_screen_clicked, this));

	_check_all->Bind     (wxEVT_BUTTON, boost::bind(&ScreensPanel::check_all, this));
	_uncheck_all->Bind   (wxEVT_BUTTON, boost::bind(&ScreensPanel::uncheck_all, this));

	SetSizer(_overall_sizer);
}


ScreensPanel::~ScreensPanel ()
{
	_targets->Unbind (wxEVT_TREELIST_SELECTION_CHANGED, &ScreensPanel::selection_changed_shim, this);
	_targets->Unbind (wxEVT_TREELIST_ITEM_CHECKED, &ScreensPanel::checkbox_changed, this);
}


void
ScreensPanel::check_all ()
{
	for (auto cinema = _targets->GetFirstChild(_targets->GetRootItem()); cinema.IsOk();  cinema = _targets->GetNextSibling(cinema)) {
		_targets->CheckItem(cinema, wxCHK_CHECKED);
		for (auto screen = _targets->GetFirstChild(cinema); screen.IsOk(); screen = _targets->GetNextSibling(screen)) {
			_targets->CheckItem(screen, wxCHK_CHECKED);
			set_screen_checked(screen, true);
		}
	}
}


void
ScreensPanel::uncheck_all ()
{
	for (auto cinema = _targets->GetFirstChild(_targets->GetRootItem()); cinema.IsOk();  cinema = _targets->GetNextSibling(cinema)) {
		_targets->CheckItem(cinema, wxCHK_UNCHECKED);
		for (auto screen = _targets->GetFirstChild(cinema); screen.IsOk(); screen = _targets->GetNextSibling(screen)) {
			_targets->CheckItem(screen, wxCHK_UNCHECKED);
			set_screen_checked(screen, false);
		}
	}
}


void
ScreensPanel::setup_sensitivity ()
{
	bool const sc = _selected_cinemas.size() == 1;
	bool const ss = _selected_screens.size() == 1;

	_edit_cinema->Enable (sc || ss);
	_remove_cinema->Enable (_selected_cinemas.size() >= 1);

	_add_screen->Enable (sc || ss);
	_edit_screen->Enable (ss);
	_remove_screen->Enable (_selected_screens.size() >= 1);

	_show_only_checked->Enable(!_checked_screens.empty());
}


void
ScreensPanel::convert_to_lower(string& s)
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}


bool
ScreensPanel::matches_search(Cinema const& cinema, string search)
{
	if (search.empty()) {
		return true;
	}

	return _collator.find(search, cinema.name);
}


/** Add an existing cinema to the GUI */
optional<wxTreeListItem>
ScreensPanel::add_cinema(CinemaID cinema_id, Cinema const& cinema, wxTreeListItem previous)
{
	CinemaList cinemas;
	auto const search = wx_to_std(_search->GetValue());
	if (!matches_search(cinema, search)) {
		return {};
	}

	auto screens = cinemas.screens(cinema_id);

	if (_show_only_checked->get()) {
		auto iter = std::find_if(screens.begin(), screens.end(), [this](pair<ScreenID, dcpomatic::Screen> const& screen) {
			auto iter2 = std::find_if(_checked_screens.begin(), _checked_screens.end(), [screen](pair<CinemaID, ScreenID> const& checked) {
				return checked.second == screen.first;
			});
			return iter2 != _checked_screens.end();
		});
		if (iter == screens.end()) {
			return {};
		}
	}

	auto id = _targets->InsertItem(_targets->GetRootItem(), previous, std_to_wx(cinema.name));

	_item_to_cinema.emplace(make_pair(id, cinema_id));
	_cinema_to_item[cinema_id] = id;

	for (auto screen: screens) {
		add_screen(cinema_id, screen.first, screen.second);
	}

	return id;
}


/** Add an existing screen to the GUI */
optional<wxTreeListItem>
ScreensPanel::add_screen(CinemaID cinema_id, ScreenID screen_id, Screen const& screen)
{
	auto item = cinema_to_item(cinema_id);
	if (!item) {
		return {};
	}

	CinemaList cinemas;

	auto id = _targets->AppendItem(*item, std_to_wx(screen.name));

	_item_to_screen.emplace(make_pair(id, make_pair(cinema_id, screen_id)));
	_screen_to_item[screen_id] = id;

	return item;
}


void
ScreensPanel::add_cinema_clicked ()
{
	CinemaDialog dialog(GetParent(), _("Add Cinema"));

	if (dialog.ShowModal() == wxID_OK) {
		auto cinema = Cinema(dialog.name(), dialog.emails(), dialog.notes(), dialog.utc_offset());

		CinemaList cinemas;
		auto existing_cinemas = cinemas.cinemas();

		auto const cinema_id = cinemas.add_cinema(cinema);

		wxTreeListItem previous = wxTLI_FIRST;
		bool found = false;
		auto const search = wx_to_std(_search->GetValue());
		for (auto existing_cinema: existing_cinemas) {
			if (!matches_search(existing_cinema.second, search)) {
				continue;
			}
			if (_collator.compare(dialog.name(), existing_cinema.second.name) < 0) {
				/* existing_cinema should be after the one we're inserting */
				found = true;
				break;
			}
			auto item = cinema_to_item(existing_cinema.first);
			DCPOMATIC_ASSERT(item);
			previous = *item;
		}

		auto item = add_cinema(cinema_id, cinema, found ? previous : wxTLI_LAST);

		if (item) {
			_targets->UnselectAll ();
			_targets->Select (*item);
		}
	}

	selection_changed ();
}


optional<CinemaID>
ScreensPanel::cinema_for_operation () const
{
	if (_selected_cinemas.size() == 1) {
		return _selected_cinemas[0];
	} else if (_selected_screens.size() == 1) {
		return _selected_screens[0].first;
	}

	return {};
}


void
ScreensPanel::edit_cinema_clicked ()
{
	auto cinema_id = cinema_for_operation();
	if (cinema_id) {
		edit_cinema(*cinema_id);
	}
}


void
ScreensPanel::edit_cinema(CinemaID cinema_id)
{
	CinemaList cinemas;
	auto cinema = cinemas.cinema(cinema_id);
	DCPOMATIC_ASSERT(cinema);

	CinemaDialog dialog(GetParent(), _("Edit cinema"), cinema->name, cinema->emails, cinema->notes, cinema->utc_offset);

	if (dialog.ShowModal() == wxID_OK) {
		cinema->emails = dialog.emails();
		cinema->name = dialog.name();
		cinema->notes = dialog.notes();
		cinema->utc_offset = dialog.utc_offset();
		cinemas.update_cinema(cinema_id, *cinema);
		auto item = cinema_to_item(cinema_id);
		DCPOMATIC_ASSERT(item);
		_targets->SetItemText (*item, std_to_wx(dialog.name()));
	}
}


void
ScreensPanel::remove_cinema_clicked ()
{
	CinemaList cinemas;

	if (_selected_cinemas.size() == 1) {
		auto cinema = cinemas.cinema(_selected_cinemas[0]);
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the cinema '%s'?"), std_to_wx(cinema->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d cinemas?"), int(_selected_cinemas.size())))) {
			return;
		}
	}

	auto cinemas_to_remove = _selected_cinemas;

	for (auto const& cinema_id: cinemas_to_remove) {
		for (auto screen: cinemas.screens(cinema_id)) {
			_checked_screens.erase({cinema_id, screen.first});
		}
		cinemas.remove_cinema(cinema_id);
		auto item = cinema_to_item(cinema_id);
		DCPOMATIC_ASSERT(item);
		_targets->DeleteItem(*item);
	}

	selection_changed ();
	setup_show_only_checked();
}


void
ScreensPanel::add_screen_clicked ()
{
	auto cinema_id = cinema_for_operation();
	if (!cinema_id) {
		return;
	}

	ScreenDialog dialog(GetParent(), _("Add Screen"));

	if (dialog.ShowModal () != wxID_OK) {
		return;
	}

	CinemaList cinemas;

	for (auto screen: cinemas.screens(*cinema_id)) {
		if (screen.second.name == dialog.name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot add a screen called '%s' as the cinema already has a screen with this name."),
					std_to_wx(dialog.name()).data()
					)
				);
			return;
		}
	}

	auto screen = Screen(dialog.name(), dialog.notes(), dialog.recipient(), dialog.recipient_file(), dialog.trusted_devices());
	auto const screen_id = cinemas.add_screen(*cinema_id, screen);

	auto const id = add_screen(*cinema_id, screen_id, screen);
	if (id) {
		_targets->Expand (id.get ());
	}
}


void
ScreensPanel::edit_screen_clicked ()
{
	if (_selected_screens.size() == 1) {
		edit_screen(_selected_screens[0].first, _selected_screens[0].second);
	}
}


void
ScreensPanel::edit_screen(CinemaID cinema_id, ScreenID screen_id)
{
	CinemaList cinemas;
	auto screen = cinemas.screen(screen_id);
	DCPOMATIC_ASSERT(screen);

	ScreenDialog dialog(
		GetParent(), _("Edit screen"),
		screen->name,
		screen->notes,
		screen->recipient,
		screen->recipient_file,
		screen->trusted_devices
		);

	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	for (auto screen: cinemas.screens(cinema_id)) {
		if (screen.first != screen_id && screen.second.name == dialog.name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot change this screen's name to '%s' as the cinema already has a screen with this name."),
					std_to_wx(dialog.name()).data()
					)
				);
			return;
		}
	}

	screen->name = dialog.name();
	screen->notes = dialog.notes();
	screen->recipient = dialog.recipient();
	screen->recipient_file = dialog.recipient_file();
	screen->trusted_devices = dialog.trusted_devices();
	cinemas.update_screen(cinema_id, screen_id, *screen);

	auto item = screen_to_item(screen_id);
	DCPOMATIC_ASSERT (item);
	_targets->SetItemText(*item, std_to_wx(dialog.name()));
}


void
ScreensPanel::remove_screen_clicked ()
{
	CinemaList cinemas;

	if (_selected_screens.size() == 1) {
		auto screen = cinemas.screen(_selected_screens[0].second);
		DCPOMATIC_ASSERT(screen);
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the screen '%s'?"), std_to_wx(screen->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d screens?"), int(_selected_screens.size())))) {
			return;
		}
	}

	for (auto screen_id: _selected_screens) {
		_checked_screens.erase(screen_id);
		cinemas.remove_screen(screen_id.second);
		auto item = screen_to_item(screen_id.second);
		DCPOMATIC_ASSERT(item);
		_targets->DeleteItem(*item);
	}

	/* This is called by the signal on Linux, but not it seems on Windows, so we call it ourselves
	 * as well.
	 */
	selection_changed();
	setup_show_only_checked();
}


std::set<pair<CinemaID, ScreenID>>
ScreensPanel::screens () const
{
	return _checked_screens;
}


void
ScreensPanel::selection_changed_shim (wxTreeListEvent &)
{
	selection_changed ();
}


void
ScreensPanel::selection_changed ()
{
	if (_ignore_selection_change) {
		return;
	}

	wxTreeListItems selection;
	_targets->GetSelections (selection);

	_selected_cinemas.clear ();
	_selected_screens.clear ();

	for (size_t i = 0; i < selection.size(); ++i) {
		if (auto cinema = item_to_cinema(selection[i])) {
			_selected_cinemas.push_back(*cinema);
		}
		if (auto screen = item_to_screen(selection[i])) {
			_selected_screens.push_back(*screen);
		}
	}

	setup_sensitivity ();
}


void
ScreensPanel::add_cinemas ()
{
	for (auto cinema: _cinema_list.cinemas()) {
		add_cinema(cinema.first, cinema.second, wxTLI_LAST);
	}
}


void
ScreensPanel::clear_and_re_add()
{
	_targets->DeleteAllItems ();

	_item_to_cinema.clear ();
	_cinema_to_item.clear ();
	_item_to_screen.clear ();
	_screen_to_item.clear ();

	add_cinemas ();
}


/** Search and/or "show only checked" changed */
void
ScreensPanel::display_filter_changed()
{
	clear_and_re_add();

	_ignore_selection_change = true;

	for (auto const& selection: _selected_cinemas) {
		if (auto item = cinema_to_item(selection)) {
			_targets->Select (*item);
		}
	}

	for (auto const& selection: _selected_screens) {
		if (auto item = screen_to_item(selection.second)) {
			_targets->Select (*item);
		}
	}

	_ignore_selection_change = false;

	_ignore_check_change = true;

	for (auto const& checked: _checked_screens) {
		if (auto item = screen_to_item(checked.second)) {
			_targets->CheckItem(*item, wxCHK_CHECKED);
			setup_cinema_checked_state(*item);
		}
	}

	_ignore_check_change = false;
}


void
ScreensPanel::set_screen_checked (wxTreeListItem item, bool checked)
{
	auto screen = item_to_screen(item);
	DCPOMATIC_ASSERT(screen);
	if (checked) {
		_checked_screens.insert({screen->first, screen->second});
	} else {
		_checked_screens.erase({screen->first, screen->second});
	}

	setup_show_only_checked();
}


void
ScreensPanel::setup_cinema_checked_state (wxTreeListItem screen)
{
	auto cinema = _targets->GetItemParent(screen);
	DCPOMATIC_ASSERT (cinema.IsOk());
	int checked = 0;
	int unchecked = 0;
	for (auto child = _targets->GetFirstChild(cinema); child.IsOk(); child = _targets->GetNextSibling(child)) {
		if (_targets->GetCheckedState(child) == wxCHK_CHECKED) {
		    ++checked;
		} else {
		    ++unchecked;
		}
	}
	if (checked == 0) {
		_targets->CheckItem(cinema, wxCHK_UNCHECKED);
	} else if (unchecked == 0) {
		_targets->CheckItem(cinema, wxCHK_CHECKED);
	} else {
		_targets->CheckItem(cinema, wxCHK_UNDETERMINED);
	}
}


void
ScreensPanel::checkbox_changed (wxTreeListEvent& ev)
{
	if (_ignore_check_change) {
		return;
	}

	if (item_to_cinema(ev.GetItem())) {
		/* Cinema: check/uncheck all children */
		auto const checked = _targets->GetCheckedState(ev.GetItem());
		for (auto child = _targets->GetFirstChild(ev.GetItem()); child.IsOk(); child = _targets->GetNextSibling(child)) {
			_targets->CheckItem(child, checked);
			set_screen_checked(child, checked);
		}
	} else {
		set_screen_checked(ev.GetItem(), _targets->GetCheckedState(ev.GetItem()));
		setup_cinema_checked_state(ev.GetItem());
	}

	ScreensChanged ();
}


optional<CinemaID>
ScreensPanel::item_to_cinema (wxTreeListItem item) const
{
	auto iter = _item_to_cinema.find (item);
	if (iter == _item_to_cinema.end()) {
		return {};
	}

	return iter->second;
}


optional<pair<CinemaID, ScreenID>>
ScreensPanel::item_to_screen (wxTreeListItem item) const
{
	auto iter = _item_to_screen.find (item);
	if (iter == _item_to_screen.end()) {
		return {};
	}

	return iter->second;
}


optional<wxTreeListItem>
ScreensPanel::cinema_to_item(CinemaID cinema) const
{
	auto iter = _cinema_to_item.find (cinema);
	if (iter == _cinema_to_item.end()) {
		return {};
	}

	return iter->second;
}


optional<wxTreeListItem>
ScreensPanel::screen_to_item(ScreenID screen) const
{
	auto iter = _screen_to_item.find (screen);
	if (iter == _screen_to_item.end()) {
		return {};
	}

	return iter->second;
}


void
ScreensPanel::item_activated(wxTreeListEvent& ev)
{
	auto iter = _item_to_cinema.find(ev.GetItem());
	if (iter != _item_to_cinema.end()) {
		edit_cinema(iter->second);
	} else {
		auto iter = _item_to_screen.find(ev.GetItem());
		if (iter != _item_to_screen.end()) {
			edit_screen(iter->second.first, iter->second.second);
		}
	}
}


void
ScreensPanel::setup_show_only_checked()
{
	if (_checked_screens.empty()) {
		_show_only_checked->set_text(_("Show only checked"));
	} else {
		_show_only_checked->set_text(wxString::Format(_("Show only %d checked"), static_cast<int>(_checked_screens.size())));
	}

	_overall_sizer->Layout();
	setup_sensitivity();
}


dcp::UTCOffset
ScreensPanel::best_utc_offset() const
{
	std::set<CinemaID> unique_cinema_ids;
	for (auto const& screen: screens()) {
		unique_cinema_ids.insert(screen.first);
	}

	CinemaList cinema_list;
	return cinema_list.unique_utc_offset(unique_cinema_ids).get_value_or(dcp::UTCOffset());
}

