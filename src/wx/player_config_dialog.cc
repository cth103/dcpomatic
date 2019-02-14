/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

/** @file src/player_config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic Player configuration.
 */

#include "config_dialog.h"
#include "wx_util.h"
#include "editable_list.h"
#include "filter_dialog.h"
#include "file_picker_ctrl.h"
#include "dir_picker_ctrl.h"
#include "isdcf_metadata_dialog.h"
#include "server_dialog.h"
#include "make_chain_dialog.h"
#include "email_dialog.h"
#include "name_format_editor.h"
#include "nag_dialog.h"
#include "monitor_dialog.h"
#include "check_box.h"
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/log.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include <dcp/locale_convert.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/stdpaths.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include <RtAudio.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::vector;
using std::string;
using std::list;
using std::cout;
using std::pair;
using std::make_pair;
using std::map;
using boost::bind;
using boost::shared_ptr;
using boost::function;
using boost::optional;
using dcp::locale_convert;

class PlayerGeneralPage : public GeneralPage
{
public:
	PlayerGeneralPage (wxSize panel_size, int border)
		: GeneralPage (panel_size, border)
	{}

private:
	void setup ()
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls (table, r);
		add_play_sound_controls (table, r);
		add_update_controls (table, r);

		add_label_to_sizer (table, _panel, _("Start player as"), true, wxGBPosition(r, 0));
		_player_mode = new wxChoice (_panel, wxID_ANY);
		_player_mode->Append (_("window"));
		_player_mode->Append (_("full screen"));
		_player_mode->Append (_("full screen with controls on other monitor"));
		table->Add (_player_mode, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Dual-screen displays"), true, wxGBPosition(r, 0));
		_image_display = new wxChoice (_panel, wxID_ANY);
		_image_display->Append (_("Image on primary, controls on secondary"));
		_image_display->Append (_("Image on secondary, controls on primary"));
		table->Add (_image_display, wxGBPosition(r, 1));
		++r;

		_respect_kdm = new CheckBox (_panel, _("Respect KDM validity periods"));
		table->Add (_respect_kdm, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

		add_label_to_sizer (table, _panel, _("Activity log file"), true, wxGBPosition (r, 0));
		_activity_log_file = new FilePickerCtrl (_panel, _("Select activity log file"), "*", true);
		table->Add (_activity_log_file, wxGBPosition(r, 1));
		++r;

#ifdef DCPOMATIC_VARIANT_SWAROOP
		add_label_to_sizer (table, _panel, _("KDM server URL"), true, wxGBPosition(r, 0));
		_kdm_server_url = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(400, -1));
		table->Add (_kdm_server_url, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Lock file"), true, wxGBPosition(r, 0));
		_lock_file = new FilePickerCtrl (_panel, _("Select lock file"), "*", true);
		table->Add (_lock_file, wxGBPosition (r, 1));
		++r;
#endif

		_player_mode->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::player_mode_changed, this));
		_image_display->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::image_display_changed, this));
		_respect_kdm->Bind (wxEVT_CHECKBOX, bind(&PlayerGeneralPage::respect_kdm_changed, this));
		_activity_log_file->Bind (wxEVT_FILEPICKER_CHANGED, bind(&PlayerGeneralPage::activity_log_file_changed, this));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		_kdm_server_url->Bind (wxEVT_TEXT, bind(&PlayerGeneralPage::kdm_server_url_changed, this));
		_lock_file->Bind (wxEVT_FILEPICKER_CHANGED, bind(&PlayerGeneralPage::lock_file_changed, this));
#endif
	}

	void config_changed ()
	{
		GeneralPage::config_changed ();

		Config* config = Config::instance ();

		switch (config->player_mode()) {
		case Config::PLAYER_MODE_WINDOW:
			checked_set (_player_mode, 0);
			break;
		case Config::PLAYER_MODE_FULL:
			checked_set (_player_mode, 1);
			break;
		case Config::PLAYER_MODE_DUAL:
			checked_set (_player_mode, 2);
			break;
		}

		checked_set (_image_display, config->image_display());
		checked_set (_respect_kdm, config->respect_kdm_validity_periods());
		if (config->player_activity_log_file()) {
			checked_set (_activity_log_file, *config->player_activity_log_file());
		}
#ifdef DCPOMATIC_VARIANT_SWAROOP
		checked_set (_kdm_server_url, config->kdm_server_url());
		if (config->player_lock_file()) {
			checked_set (_lock_file, config->player_lock_file().get());
		}
#endif
	}

private:
	void player_mode_changed ()
	{
		switch (_player_mode->GetSelection()) {
		case 0:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_WINDOW);
			break;
		case 1:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_FULL);
			break;
		case 2:
			Config::instance()->set_player_mode(Config::PLAYER_MODE_DUAL);
			break;
		}
	}

	void image_display_changed ()
	{
		Config::instance()->set_image_display(_image_display->GetSelection());
	}

	void respect_kdm_changed ()
	{
		Config::instance()->set_respect_kdm_validity_periods(_respect_kdm->GetValue());
	}

	void activity_log_file_changed ()
	{
		Config::instance()->set_player_activity_log_file(wx_to_std(_activity_log_file->GetPath()));
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void kdm_server_url_changed ()
	{
		Config::instance()->set_kdm_server_url(wx_to_std(_kdm_server_url->GetValue()));
	}

	void lock_file_changed ()
	{
		Config::instance()->set_player_lock_file(wx_to_std(_lock_file->GetPath()));
	}
#endif

	wxChoice* _player_mode;
	wxChoice* _image_display;
	wxCheckBox* _respect_kdm;
	FilePickerCtrl* _activity_log_file;
#ifdef DCPOMATIC_VARIANT_SWAROOP
	wxTextCtrl* _kdm_server_url;
	FilePickerCtrl* _lock_file;
#endif
};

class LocationsPage : public StandardPage
{
public:
	LocationsPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Locations");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("locations", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:
	void setup ()
	{

		int r = 0;

		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer (table, _panel, _("Content directory"), true, wxGBPosition (r, 0));
		_content_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
		table->Add (_content_directory, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Playlist directory"), true, wxGBPosition (r, 0));
		_playlist_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
		table->Add (_playlist_directory, wxGBPosition (r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("KDM directory"), true, wxGBPosition (r, 0));
		_kdm_directory = new wxDirPickerCtrl (_panel, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
		table->Add (_kdm_directory, wxGBPosition (r, 1));
		++r;

#ifdef DCPOMATIC_VARIANT_SWAROOP
		add_label_to_sizer (table, _panel, _("Background image"), true, wxGBPosition (r, 0));
		_background_image = new FilePickerCtrl (_panel, _("Select image file"), "*.png;*.jpg;*.jpeg;*.tif;*.tiff", true);
		table->Add (_background_image, wxGBPosition (r, 1));
		++r;
#endif

		_content_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::content_directory_changed, this));
		_playlist_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::playlist_directory_changed, this));
		_kdm_directory->Bind (wxEVT_DIRPICKER_CHANGED, bind(&LocationsPage::kdm_directory_changed, this));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		_background_image->Bind (wxEVT_FILEPICKER_CHANGED, bind(&LocationsPage::background_image_changed, this));
#endif
	}

	void config_changed ()
	{
		Config* config = Config::instance ();

		if (config->player_content_directory()) {
			checked_set (_content_directory, *config->player_content_directory());
		}
		if (config->player_playlist_directory()) {
			checked_set (_playlist_directory, *config->player_playlist_directory());
		}
		if (config->player_kdm_directory()) {
			checked_set (_kdm_directory, *config->player_kdm_directory());
		}
#ifdef DCPOMATIC_VARIANT_SWAROOP
		if (config->player_background_image()) {
			checked_set (_background_image, *config->player_background_image());
		}
#endif
	}

	void content_directory_changed ()
	{
		Config::instance()->set_player_content_directory(wx_to_std(_content_directory->GetPath()));
	}

	void playlist_directory_changed ()
	{
		Config::instance()->set_player_playlist_directory(wx_to_std(_playlist_directory->GetPath()));
	}

	void kdm_directory_changed ()
	{
		Config::instance()->set_player_kdm_directory(wx_to_std(_kdm_directory->GetPath()));
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void background_image_changed ()
	{
		boost::filesystem::path const f = wx_to_std(_background_image->GetPath());
		if (!boost::filesystem::is_regular_file(f) || !wxImage::CanRead(std_to_wx(f.string()))) {
			error_dialog (0, _("Could not load image file."));
			if (Config::instance()->player_background_image()) {
				checked_set (_background_image, *Config::instance()->player_background_image());
			}
			return;
		}

		Config::instance()->set_player_background_image(f);
	}
#endif

	wxDirPickerCtrl* _content_directory;
	wxDirPickerCtrl* _playlist_directory;
	wxDirPickerCtrl* _kdm_directory;
#ifdef DCPOMATIC_VARIANT_SWAROOP
	FilePickerCtrl* _background_image;
#endif
};

#ifdef DCPOMATIC_VARIANT_SWAROOP
class WatermarkPage : public StandardPage
{
public:
	WatermarkPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Watermark");
	}

private:
	void setup ()
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;

		add_label_to_sizer (table, _panel, _("Theatre name"), true, wxGBPosition(r, 0));
		_theatre = new wxTextCtrl (_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1));
		table->Add (_theatre, wxGBPosition(r, 1), wxGBSpan(1, 2));
		++r;

		add_label_to_sizer (table, _panel, _("Period"), true, wxGBPosition(r, 0));
		_period = new wxSpinCtrl (_panel, wxID_ANY);
		_period->SetRange (1, 60);
		table->Add (_period, wxGBPosition(r, 1));
		add_label_to_sizer (table, _panel, _("minutes"), false, wxGBPosition(r, 2));
		++r;

		add_label_to_sizer (table, _panel, _("Duration"), true, wxGBPosition(r, 0));
		_duration = new wxSpinCtrl (_panel, wxID_ANY);
		_duration->SetRange (100, 5000);
		table->Add (_duration, wxGBPosition(r, 1));
		add_label_to_sizer (table, _panel, _("milliseconds"), false, wxGBPosition(r, 2));
		++r;

		_theatre->Bind (wxEVT_TEXT, bind(&WatermarkPage::theatre_changed, this));
		_duration->Bind (wxEVT_SPINCTRL, bind(&WatermarkPage::duration_changed, this));
		_period->Bind (wxEVT_SPINCTRL, bind(&WatermarkPage::period_changed, this));
	}

	void config_changed ()
	{
		Config* config = Config::instance ();
		checked_set (_theatre, config->player_watermark_theatre());
		checked_set (_duration, config->player_watermark_duration());
		checked_set (_period, config->player_watermark_period());
	}

	void theatre_changed ()
	{
		Config::instance()->set_player_watermark_theatre(wx_to_std(_theatre->GetValue()));
	}

	void period_changed ()
	{
		Config::instance()->set_player_watermark_period(_period->GetValue());
	}

	void duration_changed ()
	{
		Config::instance()->set_player_watermark_duration(_duration->GetValue());
	}

	wxTextCtrl* _theatre;
	wxSpinCtrl* _period;
	wxSpinCtrl* _duration;
};

class DevicesPage : public StandardPage
{
public:
	DevicesPage (wxSize panel_size, int border)
		: StandardPage (panel_size, border)
	{}

	wxString GetName () const
	{
		return _("Devices");
	}

private:
	void setup ()
	{
		vector<string> columns;
		columns.push_back(wx_to_std(_("Manufacturer ID")));
		columns.push_back(wx_to_std(_("Product code")));
		columns.push_back(wx_to_std(_("Serial")));
		columns.push_back(wx_to_std(_("Manufacture week")));
		columns.push_back(wx_to_std(_("Manufacture year")));
		_monitor_list = new EditableList<Monitor, MonitorDialog> (
			_panel,
			columns,
			boost::bind (&Config::required_monitors, Config::instance()),
			boost::bind (&Config::set_required_monitors, Config::instance(), _1),
			boost::bind (&DevicesPage::monitor_column, this, _1, _2),
			true,
			true,
			100
			);
		_panel->GetSizer()->Add(_monitor_list, 1, wxEXPAND | wxALL, _border);

		wxButton* get = new Button(_panel, _("Read current devices"));
		_panel->GetSizer()->Add(get, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
		get->Bind(wxEVT_BUTTON, bind(&DevicesPage::get_clicked, this));
	}

	void get_clicked ()
	{
		Config::instance()->set_required_monitors(get_monitors());
		_monitor_list->refresh ();
	}

	string monitor_column (Monitor m, int c)
	{
		switch (c) {
		case 0:
			return m.manufacturer_id;
		case 1:
			return locale_convert<string>(m.manufacturer_product_code);
		case 2:
			return locale_convert<string>(m.serial_number);
		case 3:
			return locale_convert<string>(m.week_of_manufacture);
		case 4:
			return locale_convert<string>(m.year_of_manufacture);
		default:
			DCPOMATIC_ASSERT(false);
		}

		return "";
	}

	void config_changed ()
	{
		_monitor_list->refresh ();
	}

private:
	EditableList<Monitor, MonitorDialog>* _monitor_list;
};

#endif

wxPreferencesEditor*
create_player_config_dialog ()
{
	wxPreferencesEditor* e = new wxPreferencesEditor ();

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	wxSize ps = wxSize (520, -1);
	int const border = 16;
#else
	wxSize ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage (new PlayerGeneralPage(wxSize(-1, 500), border));
	e->AddPage (new LocationsPage(ps, border));
	e->AddPage (new KeysPage(ps, border));
#ifdef DCPOMATIC_VARIANT_SWAROOP
	e->AddPage (new WatermarkPage(ps, border));
	e->AddPage (new DevicesPage(ps, border));
#endif
	return e;
}
