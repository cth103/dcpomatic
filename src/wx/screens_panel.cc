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


#include "cinema_dialog.h"
#include "dcpomatic_button.h"
#include "screen_dialog.h"
#include "screens_panel.h"
#include "wx_util.h"
#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/scope_guard.h"
#include "lib/screen.h"
#include "lib/timer.h"
#include <unicode/putil.h>
#include <unicode/ucol.h>
#include <unicode/uiter.h>
#include <unicode/utypes.h>
#include <unicode/ustring.h>


using std::cout;
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
	auto sizer = new wxBoxSizer (wxVERTICAL);

	_search = new wxSearchCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200, search_ctrl_height()));
#ifndef __WXGTK3__
	/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
	_search->ShowCancelButton (true);
#endif
	sizer->Add (_search, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);

	auto targets = new wxBoxSizer (wxHORIZONTAL);
	_targets = new wxTreeListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_MULTIPLE | wxTL_3STATE | wxTL_NO_HEADER);
	_targets->AppendColumn (wxT("foo"));

	targets->Add (_targets, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	add_cinemas ();

	auto side_buttons = new wxBoxSizer (wxVERTICAL);

	auto target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_cinema = new Button (this, _("Add Cinema..."));
	target_buttons->Add (_add_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_cinema = new Button (this, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_cinema = new Button (this, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_add_screen = new Button (this, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_screen = new Button (this, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_screen = new Button (this, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	side_buttons->Add (target_buttons, 0, 0);

	auto check_buttons = new wxBoxSizer (wxVERTICAL);

	_check_all = new Button (this, _("Check all"));
	check_buttons->Add (_check_all, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_uncheck_all = new Button (this, _("Uncheck all"));
	check_buttons->Add (_uncheck_all, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	side_buttons->Add (check_buttons, 1, wxEXPAND | wxTOP, DCPOMATIC_BUTTON_STACK_GAP * 8);

	targets->Add (side_buttons, 0, 0);

	sizer->Add (targets, 1, wxEXPAND);

	_search->Bind        (wxEVT_TEXT, boost::bind (&ScreensPanel::search_changed, this));
	_targets->Bind       (wxEVT_TREELIST_SELECTION_CHANGED, &ScreensPanel::selection_changed_shim, this);
	_targets->Bind       (wxEVT_TREELIST_ITEM_CHECKED, &ScreensPanel::checkbox_changed, this);

	_add_cinema->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_cinema_clicked, this));
	_edit_cinema->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_cinema_clicked, this));
	_remove_cinema->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_cinema_clicked, this));

	_add_screen->Bind    (wxEVT_BUTTON, boost::bind (&ScreensPanel::add_screen_clicked, this));
	_edit_screen->Bind   (wxEVT_BUTTON, boost::bind (&ScreensPanel::edit_screen_clicked, this));
	_remove_screen->Bind (wxEVT_BUTTON, boost::bind (&ScreensPanel::remove_screen_clicked, this));

	_check_all->Bind     (wxEVT_BUTTON, boost::bind(&ScreensPanel::check_all, this));
	_uncheck_all->Bind   (wxEVT_BUTTON, boost::bind(&ScreensPanel::uncheck_all, this));

	SetSizer (sizer);

	UErrorCode status = U_ZERO_ERROR;
	_collator = ucol_open(nullptr, &status);
	if (_collator) {
		ucol_setAttribute(_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
		ucol_setAttribute(_collator, UCOL_STRENGTH, UCOL_PRIMARY, &status);
		ucol_setAttribute(_collator, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
	}

	_config_connection = Config::instance()->Changed.connect(boost::bind(&ScreensPanel::config_changed, this, _1));
}


ScreensPanel::~ScreensPanel ()
{
	_targets->Unbind (wxEVT_TREELIST_SELECTION_CHANGED, &ScreensPanel::selection_changed_shim, this);
	_targets->Unbind (wxEVT_TREELIST_ITEM_CHECKED, &ScreensPanel::checkbox_changed, this);

	if (_collator) {
		ucol_close (_collator);
	}
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
}


optional<wxTreeListItem>
ScreensPanel::add_cinema (shared_ptr<Cinema> cinema, wxTreeListItem previous)
{
	auto search = wx_to_std (_search->GetValue ());
	transform (search.begin(), search.end(), search.begin(), ::tolower);

	if (!search.empty ()) {
		auto name = cinema->name;
		transform (name.begin(), name.end(), name.begin(), ::tolower);
		if (name.find (search) == string::npos) {
			return {};
		}
	}

	auto id = _targets->InsertItem(_targets->GetRootItem(), previous, std_to_wx(cinema->name));

	_item_to_cinema[id] = cinema;
	_cinema_to_item[cinema] = id;

	for (auto screen: cinema->screens()) {
		add_screen (cinema, screen);
	}

	return id;
}


optional<wxTreeListItem>
ScreensPanel::add_screen (shared_ptr<Cinema> cinema, shared_ptr<Screen> screen)
{
	auto item = cinema_to_item(cinema);
	if (!item) {
		return {};
	}

	auto id = _targets->AppendItem(*item, std_to_wx(screen->name));

	_item_to_screen[id] = screen;
	_screen_to_item[screen] = id;

	return item;
}


void
ScreensPanel::add_cinema_clicked ()
{
	auto dialog = new CinemaDialog (GetParent(), _("Add Cinema"));
	ScopeGuard sg = [dialog]() { dialog->Destroy(); };

	if (dialog->ShowModal() == wxID_OK) {
		auto cinema = make_shared<Cinema>(dialog->name(), dialog->emails(), dialog->notes(), dialog->utc_offset_hour(), dialog->utc_offset_minute());

		auto cinemas = Config::instance()->cinemas();
		cinemas.sort(
			[this](shared_ptr<Cinema> a, shared_ptr<Cinema> b) { return compare(a->name, b->name) < 0; }
			);

		try {
			_ignore_cinemas_changed = true;
			ScopeGuard sg = [this]() { _ignore_cinemas_changed = false; };
			Config::instance()->add_cinema(cinema);
		} catch (FileError& e) {
			error_dialog(GetParent(), _("Could not write cinema details to the cinemas.xml file.  Check that the location of cinemas.xml is valid in DCP-o-matic's preferences."), std_to_wx(e.what()));
			return;
		}

		optional<wxTreeListItem> item;
		for (auto existing_cinema: cinemas) {
			if (!item && compare(dialog->name(), existing_cinema->name) < 0) {
				if (auto existing_item = cinema_to_item(existing_cinema)) {
					item = add_cinema (cinema, *existing_item);
				}
			}
		}

		if (!item) {
			item = add_cinema (cinema, wxTLI_LAST);
		}

		if (item) {
			_targets->UnselectAll ();
			_targets->Select (*item);
		}
	}

	selection_changed ();
}


shared_ptr<Cinema>
ScreensPanel::cinema_for_operation () const
{
	if (_selected_cinemas.size() == 1) {
		return _selected_cinemas[0];
	} else if (_selected_screens.size() == 1) {
		return _selected_screens[0]->cinema;
	}

	return {};
}


void
ScreensPanel::edit_cinema_clicked ()
{
	auto cinema = cinema_for_operation ();
	if (!cinema) {
		return;
	}

	auto dialog = new CinemaDialog(
		GetParent(), _("Edit cinema"), cinema->name, cinema->emails, cinema->notes, cinema->utc_offset_hour(), cinema->utc_offset_minute()
		);
	ScopeGuard sg = [dialog]() { dialog->Destroy(); };

	if (dialog->ShowModal() == wxID_OK) {
		cinema->name = dialog->name();
		cinema->emails = dialog->emails();
		cinema->notes = dialog->notes();
		cinema->set_utc_offset_hour(dialog->utc_offset_hour());
		cinema->set_utc_offset_minute(dialog->utc_offset_minute());
		notify_cinemas_changed();
		auto item = cinema_to_item(cinema);
		DCPOMATIC_ASSERT(item);
		_targets->SetItemText (*item, std_to_wx(dialog->name()));
	}
}


void
ScreensPanel::remove_cinema_clicked ()
{
	if (_selected_cinemas.size() == 1) {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the cinema '%s'?"), std_to_wx(_selected_cinemas[0]->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d cinemas?"), int(_selected_cinemas.size())))) {
			return;
		}
	}

	for (auto const& cinema: _selected_cinemas) {
		_ignore_cinemas_changed = true;
		ScopeGuard sg = [this]() { _ignore_cinemas_changed = false; };
		Config::instance()->remove_cinema(cinema);
		auto item = cinema_to_item(cinema);
		DCPOMATIC_ASSERT(item);
		_targets->DeleteItem(*item);
	}

	selection_changed ();
}


void
ScreensPanel::add_screen_clicked ()
{
	auto cinema = cinema_for_operation ();
	if (!cinema) {
		return;
	}

	auto dialog = new ScreenDialog(GetParent(), _("Add Screen"));
	ScopeGuard sg = [dialog]() { dialog->Destroy(); };

	if (dialog->ShowModal () != wxID_OK) {
		return;
	}

	for (auto screen: cinema->screens()) {
		if (screen->name == dialog->name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot add a screen called '%s' as the cinema already has a screen with this name."),
					std_to_wx(dialog->name()).data()
					)
				);
			return;
		}
	}

	auto screen = std::make_shared<Screen>(dialog->name(), dialog->notes(), dialog->recipient(), dialog->recipient_file(), dialog->trusted_devices());
	cinema->add_screen (screen);
	notify_cinemas_changed();

	auto id = add_screen (cinema, screen);
	if (id) {
		_targets->Expand (id.get ());
	}
}


void
ScreensPanel::edit_screen_clicked ()
{
	if (_selected_screens.size() != 1) {
		return;
	}

	auto edit_screen = _selected_screens[0];

	auto dialog = new ScreenDialog(
		GetParent(), _("Edit screen"),
		edit_screen->name,
		edit_screen->notes,
		edit_screen->recipient,
		edit_screen->recipient_file,
		edit_screen->trusted_devices
		);
	ScopeGuard sg = [dialog]() { dialog->Destroy(); };

	if (dialog->ShowModal() != wxID_OK) {
		return;
	}

	auto cinema = edit_screen->cinema;
	for (auto screen: cinema->screens()) {
		if (screen != edit_screen && screen->name == dialog->name()) {
			error_dialog (
				GetParent(),
				wxString::Format (
					_("You cannot change this screen's name to '%s' as the cinema already has a screen with this name."),
					std_to_wx(dialog->name()).data()
					)
				);
			return;
		}
	}

	edit_screen->name = dialog->name();
	edit_screen->notes = dialog->notes();
	edit_screen->recipient = dialog->recipient();
	edit_screen->recipient_file = dialog->recipient_file();
	edit_screen->trusted_devices = dialog->trusted_devices();
	notify_cinemas_changed();

	auto item = screen_to_item(edit_screen);
	DCPOMATIC_ASSERT (item);
	_targets->SetItemText (*item, std_to_wx(dialog->name()));
}


void
ScreensPanel::remove_screen_clicked ()
{
	if (_selected_screens.size() == 1) {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove the screen '%s'?"), std_to_wx(_selected_screens[0]->name)))) {
			return;
		}
	} else {
		if (!confirm_dialog(this, wxString::Format(_("Are you sure you want to remove %d screens?"), int(_selected_screens.size())))) {
			return;
		}
	}

	for (auto const& screen: _selected_screens) {
		screen->cinema->remove_screen(screen);
		auto item = screen_to_item(screen);
		DCPOMATIC_ASSERT(item);
		_targets->DeleteItem(*item);
	}

	notify_cinemas_changed();
}


vector<shared_ptr<Screen>>
ScreensPanel::screens () const
{
	vector<shared_ptr<Screen>> output;
	std::copy (_checked_screens.begin(), _checked_screens.end(), std::back_inserter(output));
	return output;
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
			_selected_cinemas.push_back(cinema);
		}
		if (auto screen = item_to_screen(selection[i])) {
			_selected_screens.push_back(screen);
		}
	}

	setup_sensitivity ();
}


void
ScreensPanel::add_cinemas ()
{
	auto cinemas = Config::instance()->cinemas();
	cinemas.sort(
		[this](shared_ptr<Cinema> a, shared_ptr<Cinema> b) { return compare(a->name, b->name) < 0; }
		);

	for (auto cinema: cinemas) {
		add_cinema (cinema, wxTLI_LAST);
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


void
ScreensPanel::search_changed ()
{
	clear_and_re_add();

	_ignore_selection_change = true;

	for (auto const& selection: _selected_cinemas) {
		if (auto item = cinema_to_item(selection)) {
			_targets->Select (*item);
		}
	}

	for (auto const& selection: _selected_screens) {
		if (auto item = screen_to_item(selection)) {
			_targets->Select (*item);
		}
	}

	_ignore_selection_change = false;

	_ignore_check_change = true;

	for (auto const& checked: _checked_screens) {
		if (auto item = screen_to_item(checked)) {
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
		_checked_screens.insert(screen);
	} else {
		_checked_screens.erase(screen);
	}
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


shared_ptr<Cinema>
ScreensPanel::item_to_cinema (wxTreeListItem item) const
{
	auto iter = _item_to_cinema.find (item);
	if (iter == _item_to_cinema.end()) {
		return {};
	}

	return iter->second;
}


shared_ptr<Screen>
ScreensPanel::item_to_screen (wxTreeListItem item) const
{
	auto iter = _item_to_screen.find (item);
	if (iter == _item_to_screen.end()) {
		return {};
	}

	return iter->second;
}


optional<wxTreeListItem>
ScreensPanel::cinema_to_item (shared_ptr<Cinema> cinema) const
{
	auto iter = _cinema_to_item.find (cinema);
	if (iter == _cinema_to_item.end()) {
		return {};
	}

	return iter->second;
}


optional<wxTreeListItem>
ScreensPanel::screen_to_item (shared_ptr<Screen> screen) const
{
	auto iter = _screen_to_item.find (screen);
	if (iter == _screen_to_item.end()) {
		return {};
	}

	return iter->second;
}


int
ScreensPanel::compare (string const& utf8_a, string const& utf8_b)
{
	if (_collator) {
		UErrorCode error = U_ZERO_ERROR;
		boost::scoped_array<uint16_t> utf16_a(new uint16_t[utf8_a.size() + 1]);
		u_strFromUTF8(reinterpret_cast<UChar*>(utf16_a.get()), utf8_a.size() + 1, nullptr, utf8_a.c_str(), -1, &error);
		boost::scoped_array<uint16_t> utf16_b(new uint16_t[utf8_b.size() + 1]);
		u_strFromUTF8(reinterpret_cast<UChar*>(utf16_b.get()), utf8_b.size() + 1, nullptr, utf8_b.c_str(), -1, &error);
		return ucol_strcoll(_collator, reinterpret_cast<UChar*>(utf16_a.get()), -1, reinterpret_cast<UChar*>(utf16_b.get()), -1);
	} else {
		return strcoll(utf8_a.c_str(), utf8_b.c_str());
	}
}


bool
ScreensPanel::notify_cinemas_changed()
{
	_ignore_cinemas_changed = true;
	ScopeGuard sg = [this]() { _ignore_cinemas_changed = false; };

	try {
		Config::instance()->changed(Config::CINEMAS);
	} catch (FileError& e) {
		error_dialog(GetParent(), _("Could not write cinema details to the cinemas.xml file.  Check that the location of cinemas.xml is valid in DCP-o-matic's preferences."), std_to_wx(e.what()));
		return false;
	}

	return true;
}


void
ScreensPanel::config_changed(Config::Property property)
{
	if (property == Config::Property::CINEMAS && !_ignore_cinemas_changed) {
		clear_and_re_add();
	}
}


