/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "locations_preferences_page.h"
#include "wx_util.h"
#ifdef DCPOMATIC_USE_OWN_PICKER
#include "dir_picker_ctrl.h"
#else
#include <wx/filepicker.h>
#endif
#include <wx/gbsizer.h>
#include <wx/wx.h>



using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;



LocationsPage::LocationsPage(wxSize panel_size, int border)
	: Page(panel_size, border)
{

}

wxString
LocationsPage::GetName() const
{
	return _("Locations");
}

#ifdef DCPOMATIC_OSX
wxBitmap
LocationsPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("locations"), wxBITMAP_TYPE_PNG);
}
#endif

void
LocationsPage::setup()
{
	int r = 0;

	auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

	add_label_to_sizer(table, _panel, _("Content directory"), true, wxGBPosition(r, 0));
#ifdef DCPOMATIC_USE_OWN_PICKER
	_content_directory = new DirPickerCtrl(_panel);
#else
	_content_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1), wxDIRP_DEFAULT_STYLE & ~wxDIRP_DIR_MUST_EXIST);
#endif
	table->Add(_content_directory, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("Playlist directory"), true, wxGBPosition(r, 0));
#ifdef DCPOMATIC_USE_OWN_PICKER
	_playlist_directory = new DirPickerCtrl(_panel);
#else
	_playlist_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1), wxDIRP_DEFAULT_STYLE & ~wxDIRP_DIR_MUST_EXIST);
#endif
	table->Add(_playlist_directory, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(table, _panel, _("KDM directory"), true, wxGBPosition(r, 0));
#ifdef DCPOMATIC_USE_OWN_PICKER
	_kdm_directory = new DirPickerCtrl(_panel);
#else
	_kdm_directory = new wxDirPickerCtrl(_panel, wxID_ANY, wxEmptyString, char_to_wx(wxDirSelectorPromptStr), wxDefaultPosition, wxSize(300, -1), wxDIRP_DEFAULT_STYLE & ~wxDIRP_DIR_MUST_EXIST);
#endif
	table->Add(_kdm_directory, wxGBPosition(r, 1));
	++r;

#ifdef DCPOMATIC_USE_OWN_PICKER
	_content_directory->Changed.connect(bind(&LocationsPage::content_directory_changed, this));
	_playlist_directory->Changed.connect(bind(&LocationsPage::playlist_directory_changed, this));
	_kdm_directory->Changed.connect(bind(&LocationsPage::kdm_directory_changed, this));
#else
	_content_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::content_directory_changed, this));
	_playlist_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::playlist_directory_changed, this));
	_kdm_directory->Bind(wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::kdm_directory_changed, this));
#endif
}

void
LocationsPage::config_changed()
{
	auto config = Config::instance();

	if (config->player_content_directory()) {
		checked_set(_content_directory, *config->player_content_directory());
	}
	if (config->player_playlist_directory()) {
		checked_set(_playlist_directory, *config->player_playlist_directory());
	}
	if (config->player_kdm_directory()) {
		checked_set(_kdm_directory, *config->player_kdm_directory());
	}
}

void
LocationsPage::content_directory_changed()
{
	Config::instance()->set_player_content_directory(wx_to_std(_content_directory->GetPath()));
}

void
LocationsPage::playlist_directory_changed()
{
	Config::instance()->set_player_playlist_directory(wx_to_std(_playlist_directory->GetPath()));
}

void
LocationsPage::kdm_directory_changed()
{
	Config::instance()->set_player_kdm_directory(wx_to_std(_kdm_directory->GetPath()));
}
