/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "config_dialog.h"
#include "dir_picker_ctrl.h"
#include "editable_list.h"
#include "email_dialog.h"
#include "file_picker_ctrl.h"
#include "filter_dialog.h"
#include "make_chain_dialog.h"
#include "nag_dialog.h"
#include "name_format_editor.h"
#include "server_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcp_content_type.h"
#include "lib/exceptions.h"
#include "lib/filter.h"
#include "lib/log.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include <dcp/certificate_chain.h>
#include <dcp/exceptions.h>
#include <dcp/locale_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS
#include <RtAudio.h>
#include <boost/filesystem.hpp>


using std::function;
using std::list;
using std::make_pair;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;


class PlayerGeneralPage : public GeneralPage
{
public:
	PlayerGeneralPage (wxSize panel_size, int border)
		: GeneralPage (panel_size, border)
	{}

private:
	void setup () override
	{
		wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		int r = 0;
		add_language_controls (table, r);
		add_update_controls (table, r);

		add_label_to_sizer (table, _panel, _("Start player as"), true, wxGBPosition(r, 0));
		_player_mode = new wxChoice (_panel, wxID_ANY);
		_player_mode->Append (_("window"));
		_player_mode->Append (_("full screen"));
		_player_mode->Append (_("full screen with separate advanced controls"));
		table->Add (_player_mode, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Dual-screen displays"), true, wxGBPosition(r, 0));
		_image_display = new wxChoice (_panel, wxID_ANY);
		_image_display->Append (_("Image on primary, controls on secondary"));
		_image_display->Append (_("Image on secondary, controls on primary"));
		table->Add (_image_display, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer (table, _panel, _("Video display mode"), true, wxGBPosition(r, 0));
		_video_display_mode = new wxChoice (_panel, wxID_ANY);
		_video_display_mode->Append (_("Simple (safer)"));
		_video_display_mode->Append (_("OpenGL (faster)"));
		table->Add (_video_display_mode, wxGBPosition(r, 1));
		++r;

		wxStaticText* restart = add_label_to_sizer (table, _panel, _("(restart DCP-o-matic to change display mode)"), false, wxGBPosition(r, 0));
		wxFont font = restart->GetFont();
		font.SetStyle (wxFONTSTYLE_ITALIC);
		font.SetPointSize (font.GetPointSize() - 1);
		restart->SetFont (font);
		++r;

		_respect_kdm = new CheckBox (_panel, _("Respect KDM validity periods"));
		table->Add (_respect_kdm, wxGBPosition(r, 0), wxGBSpan(1, 2));
		++r;

		add_label_to_sizer (table, _panel, _("Debug log file"), true, wxGBPosition (r, 0));
		_debug_log_file = new FilePickerCtrl(_panel, _("Select debug log file"), "*", false, true, "DebugLogPath");
		table->Add (_debug_log_file, wxGBPosition(r, 1));
		++r;

		_player_mode->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::player_mode_changed, this));
		_image_display->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::image_display_changed, this));
		_video_display_mode->Bind (wxEVT_CHOICE, bind(&PlayerGeneralPage::video_display_mode_changed, this));
		_respect_kdm->bind(&PlayerGeneralPage::respect_kdm_changed, this);
		_debug_log_file->Bind (wxEVT_FILEPICKER_CHANGED, bind(&PlayerGeneralPage::debug_log_file_changed, this));
	}

	void config_changed () override
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

		switch (config->video_view_type()) {
		case Config::VIDEO_VIEW_SIMPLE:
			checked_set (_video_display_mode, 0);
			break;
		case Config::VIDEO_VIEW_OPENGL:
			checked_set (_video_display_mode, 1);
			break;
		}

		checked_set (_image_display, config->image_display());
		checked_set (_respect_kdm, config->respect_kdm_validity_periods());
		if (config->player_debug_log_file()) {
			checked_set (_debug_log_file, *config->player_debug_log_file());
		}
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

	void video_display_mode_changed ()
	{
		if (_video_display_mode->GetSelection() == 0) {
			Config::instance()->set_video_view_type (Config::VIDEO_VIEW_SIMPLE);
		} else {
			Config::instance()->set_video_view_type (Config::VIDEO_VIEW_OPENGL);
		}
	}

	void respect_kdm_changed ()
	{
		Config::instance()->set_respect_kdm_validity_periods(_respect_kdm->GetValue());
	}

	void debug_log_file_changed ()
	{
		if (auto path = _debug_log_file->path()) {
			Config::instance()->set_player_debug_log_file(*path);
		}
	}

	wxChoice* _player_mode;
	wxChoice* _image_display;
	wxChoice* _video_display_mode;
	CheckBox* _respect_kdm;
	FilePickerCtrl* _debug_log_file;
};


/** @class PlayerAdvancedPage
 *  @brief Advanced page of the preferences dialog for the player.
 */
class PlayerAdvancedPage : public Page
{
public:
	PlayerAdvancedPage (wxSize panel_size, int border)
		: Page (panel_size, border)
		, _log_general (0)
		, _log_warning (0)
		, _log_error (0)
		, _log_timing (0)
	{}

	wxString GetName () const override
	{
		return _("Advanced");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const override
	{
		return wxBitmap(icon_path("advanced"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void add_top_aligned_label_to_sizer (wxSizer* table, wxWindow* parent, wxString text)
	{
		int flags = wxALIGN_TOP | wxTOP | wxLEFT;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
		text += wxT (":");
#endif
		wxStaticText* m = new StaticText (parent, text);
		table->Add (m, 0, flags, DCPOMATIC_SIZER_Y_GAP);
	}

	void setup () override
	{
		auto table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		{
			add_top_aligned_label_to_sizer (table, _panel, _("Log"));
			wxBoxSizer* t = new wxBoxSizer (wxVERTICAL);
			_log_general = new CheckBox (_panel, _("General"));
			t->Add (_log_general, 1, wxEXPAND | wxALL);
			_log_warning = new CheckBox (_panel, _("Warnings"));
			t->Add (_log_warning, 1, wxEXPAND | wxALL);
			_log_error = new CheckBox (_panel, _("Errors"));
			t->Add (_log_error, 1, wxEXPAND | wxALL);
			/// TRANSLATORS: translate the word "Timing" here; do not include the "Config|" prefix
			_log_timing = new CheckBox (_panel, S_("Config|Timing"));
			t->Add (_log_timing, 1, wxEXPAND | wxALL);
			_log_debug_video_view = new CheckBox(_panel, _("Debug: video view"));
			t->Add(_log_debug_video_view, 1, wxEXPAND | wxALL);
			table->Add (t, 0, wxALL, 6);
		}

#ifdef DCPOMATIC_WINDOWS
		_win32_console = new CheckBox (_panel, _("Open console window"));
		table->Add (_win32_console, 1, wxEXPAND | wxALL);
		table->AddSpacer (0);
#endif

		_log_general->bind(&PlayerAdvancedPage::log_changed, this);
		_log_warning->bind(&PlayerAdvancedPage::log_changed, this);
		_log_error->bind(&PlayerAdvancedPage::log_changed, this);
		_log_timing->bind(&PlayerAdvancedPage::log_changed, this);
		_log_debug_video_view->bind(&PlayerAdvancedPage::log_changed, this);
#ifdef DCPOMATIC_WINDOWS
		_win32_console->bind(&PlayerAdvancedPage::win32_console_changed, this);
#endif
	}

	void config_changed () override
	{
		auto config = Config::instance ();

		checked_set (_log_general, config->log_types() & LogEntry::TYPE_GENERAL);
		checked_set (_log_warning, config->log_types() & LogEntry::TYPE_WARNING);
		checked_set (_log_error, config->log_types() & LogEntry::TYPE_ERROR);
		checked_set (_log_timing, config->log_types() & LogEntry::TYPE_TIMING);
#ifdef DCPOMATIC_WINDOWS
		checked_set (_win32_console, config->win32_console());
#endif
	}

	void log_changed ()
	{
		int types = 0;
		if (_log_general->GetValue ()) {
			types |= LogEntry::TYPE_GENERAL;
		}
		if (_log_warning->GetValue ()) {
			types |= LogEntry::TYPE_WARNING;
		}
		if (_log_error->GetValue ())  {
			types |= LogEntry::TYPE_ERROR;
		}
		if (_log_timing->GetValue ()) {
			types |= LogEntry::TYPE_TIMING;
		}
		if (_log_debug_video_view->GetValue()) {
			types |= LogEntry::TYPE_DEBUG_VIDEO_VIEW;
		}
		Config::instance()->set_log_types (types);
	}

#ifdef DCPOMATIC_WINDOWS
	void win32_console_changed ()
	{
		Config::instance()->set_win32_console (_win32_console->GetValue ());
	}
#endif

	CheckBox* _log_general;
	CheckBox* _log_warning;
	CheckBox* _log_error;
	CheckBox* _log_timing;
	CheckBox* _log_debug_video_view;
#ifdef DCPOMATIC_WINDOWS
	CheckBox* _win32_console;
#endif
};


wxPreferencesEditor*
create_player_config_dialog ()
{
	auto e = new wxPreferencesEditor (_("DCP-o-matic Player Preferences"));

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	auto ps = wxSize (520, -1);
	int const border = 16;
#else
	auto ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage (new PlayerGeneralPage(wxSize(-1, 500), border));
	e->AddPage (new SoundPage(ps, border));
	e->AddPage (new LocationsPage(ps, border));
	e->AddPage (new KeysPage(ps, border));
	e->AddPage (new PlayerAdvancedPage(ps, border));
	return e;
}
